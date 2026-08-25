/**
 * @file fan_lib.cpp
 * @brief Implementation of FanTesting for Raspberry Pi 5 / Compute Module 5.
 * @details Checks the PWM cooling fan exposed through the thermal cooling_device
 *          and PWM sysfs interfaces. The Pi5 fan connector (5V) is driven by the
 *          thermal engine (cooling_device0/1); carrier boards on CM5 may expose
 *          a PWM fan on /sys/class/pwm.
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */

#include "fan_lib.h"
#include "../platform_rpi5.h"
#include <dirent.h>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <vector>

static bool pathExists(const std::string& p)
{
    struct stat s;
    return stat(p.c_str(), &s) == 0;
}

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

FanTesting::FanTesting()
{
}
FanTesting::~FanTesting()
{
}

TestOutput FanTesting::testFanPresent()
{
    std::vector<std::string> data;
    data.push_back("=== Fan Presence ===");
    bool found = false;
    for (const char* cd : {RPI5_FAN_THERMAL_CD0, RPI5_FAN_THERMAL_CD1}) {
        if (pathExists(cd)) {
            std::string type = readSysfs(std::string(cd) + "/type");
            data.push_back("[INFO] Cooling device: " + std::string(cd) + " type=" + type);
            if (type.find("fan") != std::string::npos || type.find("Fan") != std::string::npos) {
                data.push_back("[PASS] Fan cooling device found");
                found = true;
            }
        }
    }
    for (const auto& c : listDir(RPI5_FAN_PWM)) {
        if (c.rfind("pwmchip", 0) == 0) {
            data.push_back("[INFO] PWM chip present (fan may use it): " + c);
        }
    }
    if (!found)
        data.push_back("[INFO] No fan cooling device detected (fan not populated / not in DT)");
    {
        TestOutput _out;
        _out.status = found ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = found ? hal::Severity::INFO : hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
}

TestOutput FanTesting::testFanThermalZone()
{
    std::vector<std::string> data;
    data.push_back("=== Fan Thermal Zone ===");
    bool read = false;
    for (const char* cd : {RPI5_FAN_THERMAL_CD0, RPI5_FAN_THERMAL_CD1}) {
        if (!pathExists(cd))
            continue;
        std::string cur = readSysfs(std::string(cd) + "/cur_state");
        std::string max = readSysfs(std::string(cd) + "/max_state");
        if (!cur.empty()) {
            data.push_back("[INFO] " + std::string(cd) + " cur_state=" + cur + " max_state=" + max);
            read = true;
        }
    }
    if (!read)
        data.push_back("[INFO] No cooling device readable");
    {
        TestOutput _out;
        _out.status = read ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = read ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

TestOutput FanTesting::testFanControl()
{
    std::vector<std::string> data;
    data.push_back("=== Fan Speed Control ===");
    bool passed = false;
    for (const char* cd : {RPI5_FAN_THERMAL_CD0, RPI5_FAN_THERMAL_CD1}) {
        if (!pathExists(cd))
            continue;
        std::string cur = readSysfs(std::string(cd) + "/cur_state");
        if (!cur.empty()) {
            data.push_back("[PASS] Fan control state read: " + cur);
            passed = true;
        }
    }
    if (!passed)
        data.push_back("[INFO] Fan control not available (no fan cooling device)");
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

std::vector<TestResult> FanTesting::runAllTests()
{
    std::vector<TestResult> results;
    const std::vector<std::pair<std::string, TestOutput (FanTesting::*)()>> tests = {
        {"testFanPresent", &FanTesting::testFanPresent},
        {"testFanThermalZone", &FanTesting::testFanThermalZone},
        {"testFanControl", &FanTesting::testFanControl},
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

std::vector<std::string> FanTesting::getAvailableTests()
{
    return {"testFanPresent", "testFanThermalZone", "testFanControl"};
}

TestResult FanTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput out;
    out.status = hal::Status::ERROR;
    out.severity = hal::Severity::ERROR;
    out.error_message = "test not dispatched";
    bool known = true;
    if (testName == "testFanPresent")
        out = testFanPresent();
    else if (testName == "testFanThermalZone")
        out = testFanThermalZone();
    else if (testName == "testFanControl")
        out = testFanControl();
    else
        known = false;
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
