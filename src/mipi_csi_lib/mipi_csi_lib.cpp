/**
 * @file mipi_csi_lib.cpp
 * @brief Implementation of MipiCsiTesting for Raspberry Pi 5.
 * @details Checks the RP1 Unicam CSI-2 controller, V4L2 camera device nodes
 *          (/dev/video*), the media controller, and format negotiation.
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */

#include "mipi_csi_lib.h"
#include "../platform_rpi5.h"
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <cstdio>

static bool pathExists(const std::string& p)
{
    struct stat s;
    return stat(p.c_str(), &s) == 0;
}

static std::string runCommand(const std::string& cmd)
{
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe)
        return result;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe))
        result += buf;
    pclose(pipe);
    return result;
}

MipiCsiTesting::MipiCsiTesting() {}
MipiCsiTesting::~MipiCsiTesting() {}

TestOutput MipiCsiTesting::testMipiCsiPresent()
{
    std::vector<std::string> data;
    data.push_back("=== MIPI CSI-2 Presence ===");
    bool passed = pathExists(RPI5_UNICAM_PATH);
    if (passed) {
        data.push_back("[PASS] RP1 Unicam CSI-2 controller present: " + std::string(RPI5_UNICAM_PATH));
    } else {
        data.push_back("[FAIL] Unicam CSI-2 controller not found at " + std::string(RPI5_UNICAM_PATH));
    }
    if (pathExists(RPI5_MEDIA_CTL0))
        data.push_back("[PASS] Media controller present: " + std::string(RPI5_MEDIA_CTL0));
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        return _out;
    }
}

TestOutput MipiCsiTesting::testMipiCsiDeviceEnumeration()
{
    std::vector<std::string> data;
    data.push_back("=== MIPI CSI Camera Devices ===");
    bool found = false;
    std::string nodes = runCommand("ls -l /dev/video* 2>/dev/null | head -10");
    if (!nodes.empty()) {
        std::istringstream iss(nodes);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty()) {
                data.push_back("[INFO] " + line);
                found = true;
            }
        }
    }
    std::string cameras = runCommand("v4l2-ctl --list-devices 2>/dev/null | head -10");
    if (!cameras.empty()) {
        data.push_back("[INFO] v4l2 devices:");
        std::istringstream iss(cameras);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty())
                data.push_back("  " + line);
        }
        found = true;
    }
    if (!found)
        data.push_back("[INFO] No V4L2 camera devices detected (attach a CSI camera)");
    {
        TestOutput _out;
        _out.status = found ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = found ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        return _out;
    }
}

TestOutput MipiCsiTesting::testMipiCsiFormatNegotiation()
{
    std::vector<std::string> data;
    data.push_back("=== MIPI CSI Format Negotiation ===");
    bool passed = false;
    std::string fmts = runCommand("v4l2-ctl -d " + std::string(RPI5_CSI_DEVICE0) +
                                  " --list-formats-ext 2>/dev/null | head -15");
    if (!fmts.empty()) {
        data.push_back("[PASS] Format negotiation available on " + std::string(RPI5_CSI_DEVICE0));
        std::istringstream iss(fmts);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty())
                data.push_back("  " + line);
        }
        passed = true;
    } else {
        data.push_back("[INFO] Cannot list formats (no camera or no v4l2-ctl)");
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

std::vector<TestResult> MipiCsiTesting::runAllTests()
{
    std::vector<TestResult> results;
    const std::vector<std::pair<std::string, TestOutput (MipiCsiTesting::*)()>> tests = {
        {"testMipiCsiPresent", &MipiCsiTesting::testMipiCsiPresent},
        {"testMipiCsiDeviceEnumeration", &MipiCsiTesting::testMipiCsiDeviceEnumeration},
        {"testMipiCsiFormatNegotiation", &MipiCsiTesting::testMipiCsiFormatNegotiation},
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

std::vector<std::string> MipiCsiTesting::getAvailableTests()
{
    return { "testMipiCsiPresent", "testMipiCsiDeviceEnumeration", "testMipiCsiFormatNegotiation" };
}

TestResult MipiCsiTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput out;
    out.status = hal::Status::ERROR;
    out.severity = hal::Severity::ERROR;
    out.error_message = "test not dispatched";
    bool known = true;
    if (testName == "testMipiCsiPresent") out = testMipiCsiPresent();
    else if (testName == "testMipiCsiDeviceEnumeration") out = testMipiCsiDeviceEnumeration();
    else if (testName == "testMipiCsiFormatNegotiation") out = testMipiCsiFormatNegotiation();
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
