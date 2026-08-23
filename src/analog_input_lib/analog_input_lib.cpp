/**
 * @file analog_input_lib.cpp
 * @brief Implementation of AnalogInputTesting for Raspberry Pi 5.
 * @details Checks IIO ADC devices (/sys/bus/iio/devices/iio:device*), reads raw
 *          channel values, and reports the number of enabled channels. Requires
 *          an external ADC (e.g. MCP3008 via SPI) - Pi5 has no onboard ADC.
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */

#include "analog_input_lib.h"
#include "../platform_rpi5.h"
#include <dirent.h>
#include <fstream>
#include <string>
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

AnalogInputTesting::AnalogInputTesting() {}
AnalogInputTesting::~AnalogInputTesting() {}

TestOutput AnalogInputTesting::testAdcDevicePresent()
{
    std::vector<std::string> data;
    data.push_back("=== ADC Device Presence ===");
    bool found = false;
    for (const auto& d : listDir(RPI5_IIO_SYSFS)) {
        if (d.rfind("iio:device", 0) == 0) {
            std::string name = readSysfs(std::string(RPI5_IIO_SYSFS) + "/" + d + "/name");
            data.push_back("[PASS] IIO device " + d + " name=" + name);
            found = true;
        }
    }
    if (!found) {
        data.push_back("[FAIL] No IIO ADC device found");
        data.push_back("[INFO] Pi5 has no onboard ADC; connect MCP3008/ADS1115 via SPI/I2C");
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

TestOutput AnalogInputTesting::testAdcChannelRead()
{
    std::vector<std::string> data;
    data.push_back("=== ADC Channel Read ===");
    bool read = false;
    for (const auto& d : listDir(RPI5_IIO_SYSFS)) {
        if (d.rfind("iio:device", 0) != 0)
            continue;
        // scan for in_voltage*_raw or in_accel*_raw files
        for (const auto& f : listDir(std::string(RPI5_IIO_SYSFS) + "/" + d)) {
            if (f.rfind("in_voltage", 0) == 0 && f.find("_raw") != std::string::npos) {
                std::string v = readSysfs(std::string(RPI5_IIO_SYSFS) + "/" + d + "/" + f);
                if (!v.empty()) {
                    data.push_back("[PASS] " + f + "=" + v);
                    read = true;
                }
            }
        }
    }
    if (!read)
        data.push_back("[INFO] No ADC channel raw values readable (no ADC attached)");
    {
        TestOutput _out;
        _out.status = read ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = read ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        return _out;
    }
}

TestOutput AnalogInputTesting::testAdcResolution()
{
    std::vector<std::string> data;
    data.push_back("=== ADC Resolution ===");
    bool read = false;
    for (const auto& d : listDir(RPI5_IIO_SYSFS)) {
        if (d.rfind("iio:device", 0) != 0)
            continue;
        std::string bits = readSysfs(std::string(RPI5_IIO_SYSFS) + "/" + d + "/in_voltage_scale");
        if (bits.empty())
            bits = readSysfs(std::string(RPI5_IIO_SYSFS) + "/" + d + "/in_voltage_resolution");
        if (!bits.empty()) {
            data.push_back("[INFO] " + d + " scale/resolution=" + bits);
            read = true;
        }
    }
    if (!read)
        data.push_back("[INFO] ADC resolution not exposed");
    {
        TestOutput _out;
        _out.status = read ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = read ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        return _out;
    }
}

std::vector<TestResult> AnalogInputTesting::runAllTests()
{
    std::vector<TestResult> results;
    const std::vector<std::pair<std::string, TestOutput (AnalogInputTesting::*)()>> tests = {
        {"testAdcDevicePresent", &AnalogInputTesting::testAdcDevicePresent},
        {"testAdcChannelRead", &AnalogInputTesting::testAdcChannelRead},
        {"testAdcResolution", &AnalogInputTesting::testAdcResolution},
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

std::vector<std::string> AnalogInputTesting::getAvailableTests()
{
    return { "testAdcDevicePresent", "testAdcChannelRead", "testAdcResolution" };
}

TestResult AnalogInputTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput out;
    out.status = hal::Status::ERROR;
    out.severity = hal::Severity::ERROR;
    out.error_message = "test not dispatched";
    bool known = true;
    if (testName == "testAdcDevicePresent") out = testAdcDevicePresent();
    else if (testName == "testAdcChannelRead") out = testAdcChannelRead();
    else if (testName == "testAdcResolution") out = testAdcResolution();
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
