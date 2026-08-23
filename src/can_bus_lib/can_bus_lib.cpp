/**
 * @file can_bus_lib.cpp
 * @brief Implementation of CanBusTesting for Raspberry Pi 5.
 * @details Checks for a CAN network interface (can0), its operational state, and
 *          transmit capability. Pi5 has no native CAN controller; a SPI/MCP2515
 *          HAT is required to expose can0.
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */

#include "can_bus_lib.h"
#include "../platform_rpi5.h"
#include <dirent.h>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <vector>

static std::string readSysfs(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open())
        return {};
    std::string val;
    std::getline(f, val);
    while (!val.empty() && (val.back() == '\n' || val.back() == '\r' || val.back() == ' '))
        val.pop_back();
    return val;
}

static std::vector<std::string> listDir(const std::string& base)
{
    std::vector<std::string> out;
    DIR* dir = opendir(base.c_str());
    if (!dir)
        return out;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string n = entry->d_name;
        if (n == "." || n == "..")
            continue;
        out.push_back(n);
    }
    closedir(dir);
    return out;
}

CanBusTesting::CanBusTesting() {}
CanBusTesting::~CanBusTesting() {}

TestOutput CanBusTesting::testCanControllerPresent()
{
    std::vector<std::string> data;
    data.push_back("=== CAN Controller Presence ===");
    bool found = false;
    for (const auto& i : listDir(RPI5_CAN_SYSFS)) {
        if (i.rfind("can", 0) == 0 || i == "can0") {
            data.push_back("[PASS] CAN interface found: " + i);
            found = true;
        }
    }
    if (!found) {
        data.push_back("[FAIL] No can* network interface present");
        data.push_back("[INFO] Pi5 has no native CAN; needs SPI MCP2515 HAT configured in /boot/firmware/config.txt");
    }
    {
        TestOutput _out;
        _out.status = found ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = found ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        return _out;
    }
}

TestOutput CanBusTesting::testCanInterfaceState()
{
    std::vector<std::string> data;
    data.push_back("=== CAN Interface State ===");
    std::string state = readSysfs(std::string(RPI5_CAN_SYSFS) + "/can0/operstate");
    bool passed = !state.empty();
    if (passed) {
        data.push_back("[INFO] can0 operstate=" + state);
        passed = (state == "up" || state == "unknown");
    } else {
        data.push_back("[FAIL] can0 operstate not readable");
    }
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        return _out;
    }
}

TestOutput CanBusTesting::testCanLinkActive()
{
    std::vector<std::string> data;
    data.push_back("=== CAN Link / Tx Counter ===");
    bool passed = false;
    std::string tx = readSysfs(std::string(RPI5_CAN_SYSFS) + "/can0/statistics/tx_packets");
    std::string rx = readSysfs(std::string(RPI5_CAN_SYSFS) + "/can0/statistics/rx_packets");
    if (!tx.empty()) {
        data.push_back("[INFO] can0 tx_packets=" + tx + " rx_packets=" + rx);
        passed = true;
    } else {
        data.push_back("[FAIL] CAN statistics not readable");
    }
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        return _out;
    }
}

std::vector<TestResult> CanBusTesting::runAllTests()
{
    std::vector<TestResult> results;
    const std::vector<std::pair<std::string, TestOutput (CanBusTesting::*)()>> tests = {
        {"testCanControllerPresent", &CanBusTesting::testCanControllerPresent},
        {"testCanInterfaceState", &CanBusTesting::testCanInterfaceState},
        {"testCanLinkActive", &CanBusTesting::testCanLinkActive},
    };
    for (const auto& t : tests) {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput out = (this->*t.second)();
        auto end = std::chrono::high_resolution_clock::now();
        TestResult _tr;
        _tr.name = t.first;
        _tr.test_id = t.first;
        _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        _tr.output = out;
        results.push_back(std::move(_tr));
    }
    return results;
}

std::vector<std::string> CanBusTesting::getAvailableTests()
{
    return { "testCanControllerPresent", "testCanInterfaceState", "testCanLinkActive" };
}

TestResult CanBusTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput out;
    out.status = hal::Status::ERROR;
    out.severity = hal::Severity::ERROR;
    out.error_message = "test not dispatched";
    bool known = true;
    if (testName == "testCanControllerPresent") out = testCanControllerPresent();
    else if (testName == "testCanInterfaceState") out = testCanInterfaceState();
    else if (testName == "testCanLinkActive") out = testCanLinkActive();
    else known = false;
    auto end = std::chrono::high_resolution_clock::now();
    TestResult _tr;
    _tr.name = known ? testName : testName + " (UNKNOWN)";
    _tr.test_id = testName;
    _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    if (!known) {
        _tr.output.status = hal::Status::ERROR;
        _tr.output.severity = hal::Severity::ERROR;
        _tr.output.error_message = "Unknown test name: " + testName;
    } else {
        _tr.output = out;
    }
    return _tr;
}
