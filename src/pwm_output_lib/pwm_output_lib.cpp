/**
 * @file pwm_output_lib.cpp
 * @brief Implementation of PwmOutputTesting for Raspberry Pi 5.
 * @details Tests PWM chips via /sys/class/pwm. Note: Pi5 RP1 does not expose a
 *          user-space PWM sysfs controller on the standard 40-pin header; PWM is
 *          usually emulated via PWM sysfs on a HAT or via pigpio. This module
 *          verifies the sysfs interface when a PWM controller is present.
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */

#include "pwm_output_lib.h"
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

static bool pathExists(const std::string& p)
{
    struct stat s;
    return stat(p.c_str(), &s) == 0;
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

PwmOutputTesting::PwmOutputTesting() {}
PwmOutputTesting::~PwmOutputTesting() {}

TestOutput PwmOutputTesting::testPwmChipPresent()
{
    std::vector<std::string> data;
    data.push_back("=== PWM Chip Presence ===");
    bool found = false;
    if (pathExists(RPI5_PWM_SYSFS)) {
        for (const auto& c : listDir(RPI5_PWM_SYSFS)) {
            if (c.rfind("pwmchip", 0) == 0) {
                data.push_back("[PASS] PWM chip: " + c);
                found = true;
            }
        }
    }
    if (!found)
        data.push_back("[INFO] No PWM chips present (RP1 header has no user PWM; HAT may add one)");
    {
        TestOutput _out;
        _out.status = found ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = found ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        return _out;
    }
}

TestOutput PwmOutputTesting::testPwmExportChannel()
{
    std::vector<std::string> data;
    data.push_back("=== PWM Channel Export ===");
    bool exported = false;
    if (pathExists(RPI5_PWM_SYSFS)) {
        for (const auto& c : listDir(RPI5_PWM_SYSFS)) {
            if (c.rfind("pwmchip", 0) != 0)
                continue;
            std::string npwm = readSysfs(std::string(RPI5_PWM_SYSFS) + "/" + c + "/npwm");
            if (!npwm.empty()) {
                data.push_back("[INFO] " + c + " npwm=" + npwm);
                exported = true;
            }
        }
    }
    if (!exported)
        data.push_back("[INFO] No PWM channel count readable");
    {
        TestOutput _out;
        _out.status = exported ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = exported ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        return _out;
    }
}

TestOutput PwmOutputTesting::testPwmDutyCycle()
{
    std::vector<std::string> data;
    data.push_back("=== PWM Duty Cycle / Frequency ===");
    bool examined = false;
    if (pathExists(RPI5_PWM_SYSFS)) {
        for (const auto& c : listDir(RPI5_PWM_SYSFS)) {
            if (c.rfind("pwmchip", 0) != 0)
                continue;
            std::string pwmRoot = std::string(RPI5_PWM_SYSFS) + "/" + c;
            // look for an already-exported pwm0 dir
            if (pathExists(pwmRoot + "/pwm0")) {
                std::string period = readSysfs(pwmRoot + "/pwm0/period");
                std::string duty = readSysfs(pwmRoot + "/pwm0/duty_cycle");
                data.push_back("[INFO] " + c + "/pwm0 period=" + period + " duty=" + duty + " ns");
                examined = true;
            }
        }
    }
    if (!examined)
        data.push_back("[INFO] No exported PWM channel to inspect duty cycle");
    {
        TestOutput _out;
        _out.status = examined ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = examined ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        return _out;
    }
}

std::vector<TestResult> PwmOutputTesting::runAllTests()
{
    std::vector<TestResult> results;
    const std::vector<std::pair<std::string, TestOutput (PwmOutputTesting::*)()>> tests = {
        {"testPwmChipPresent", &PwmOutputTesting::testPwmChipPresent},
        {"testPwmExportChannel", &PwmOutputTesting::testPwmExportChannel},
        {"testPwmDutyCycle", &PwmOutputTesting::testPwmDutyCycle},
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

std::vector<std::string> PwmOutputTesting::getAvailableTests()
{
    return { "testPwmChipPresent", "testPwmExportChannel", "testPwmDutyCycle" };
}

TestResult PwmOutputTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput out;
    out.status = hal::Status::ERROR;
    out.severity = hal::Severity::ERROR;
    out.error_message = "test not dispatched";
    bool known = true;
    if (testName == "testPwmChipPresent") out = testPwmChipPresent();
    else if (testName == "testPwmExportChannel") out = testPwmExportChannel();
    else if (testName == "testPwmDutyCycle") out = testPwmDutyCycle();
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
