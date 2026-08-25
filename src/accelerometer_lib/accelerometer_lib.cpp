/**
 * @file accelerometer_lib.cpp
 * @brief Implementation of AccelerometerTesting for Raspberry Pi 5.
 * @details Checks an IIO accelerometer device, reads raw XYZ axes, and reports the
 *          scale. Pi5 has no onboard accelerometer; a HAT (e.g. LSM6DSM) is needed.
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */

#include "accelerometer_lib.h"
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

AccelerometerTesting::AccelerometerTesting() {}
AccelerometerTesting::~AccelerometerTesting() {}

TestOutput AccelerometerTesting::testAccelerometerPresent()
{
    std::vector<std::string> data;
    data.push_back("=== Accelerometer Presence ===");
    bool found = false;
    for (const auto& d : listDir(RPI5_IIO_SYSFS)) {
        if (d.rfind("iio:device", 0) != 0)
            continue;
        std::string name = readSysfs(std::string(RPI5_IIO_SYSFS) + "/" + d + "/name");
        data.push_back("[INFO] IIO device " + d + " name=" + name);
        // check for accelerometer channels
        bool hasAccel = false;
        for (const auto& f : listDir(std::string(RPI5_IIO_SYSFS) + "/" + d)) {
            if (f.rfind("in_accel_x_raw", 0) == 0) {
                hasAccel = true;
                break;
            }
        }
        if (hasAccel) {
            data.push_back("[PASS] Accelerometer found on " + d + " (" + name + ")");
            found = true;
        }
    }
    if (!found) {
        data.push_back("[FAIL] No accelerometer IIO device found");
        data.push_back("[INFO] Pi5 has no onboard accelerometer; connect a HAT (LSM6DSM/BNO055)");
    }
    {
        TestOutput _out;
        _out.status = found ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = found ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

TestOutput AccelerometerTesting::testAccelerometerRead()
{
    std::vector<std::string> data;
    data.push_back("=== Accelerometer Axis Read ===");
    bool read = false;
    for (const auto& d : listDir(RPI5_IIO_SYSFS)) {
        if (d.rfind("iio:device", 0) != 0)
            continue;
        std::string base = std::string(RPI5_IIO_SYSFS) + "/" + d;
        for (const char* axis : { "in_accel_x_raw", "in_accel_y_raw", "in_accel_z_raw" }) {
            std::string v = readSysfs(base + "/" + axis);
            if (!v.empty()) {
                data.push_back("[PASS] " + std::string(axis) + "=" + v);
                read = true;
            }
        }
    }
    if (!read)
        data.push_back("[INFO] No accelerometer raw values readable");
    {
        TestOutput _out;
        _out.status = read ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = read ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

TestOutput AccelerometerTesting::testAccelerometerScale()
{
    std::vector<std::string> data;
    data.push_back("=== Accelerometer Scale ===");
    bool read = false;
    for (const auto& d : listDir(RPI5_IIO_SYSFS)) {
        if (d.rfind("iio:device", 0) != 0)
            continue;
        std::string base = std::string(RPI5_IIO_SYSFS) + "/" + d;
        std::string scale = readSysfs(base + "/in_accel_scale");
        if (!scale.empty()) {
            data.push_back("[PASS] in_accel_scale=" + scale);
            read = true;
        }
    }
    if (!read)
        data.push_back("[INFO] Accelerometer scale not exposed");
    {
        TestOutput _out;
        _out.status = read ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = read ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

std::vector<TestResult> AccelerometerTesting::runAllTests()
{
    std::vector<TestResult> results;
    const std::vector<std::pair<std::string, TestOutput (AccelerometerTesting::*)()>> tests = {
        {"testAccelerometerPresent", &AccelerometerTesting::testAccelerometerPresent},
        {"testAccelerometerRead", &AccelerometerTesting::testAccelerometerRead},
        {"testAccelerometerScale", &AccelerometerTesting::testAccelerometerScale},
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

std::vector<std::string> AccelerometerTesting::getAvailableTests()
{
    return { "testAccelerometerPresent", "testAccelerometerRead", "testAccelerometerScale" };
}

TestResult AccelerometerTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput out;
    out.status = hal::Status::ERROR;
    out.severity = hal::Severity::ERROR;
    out.error_message = "test not dispatched";
    bool known = true;
    if (testName == "testAccelerometerPresent") out = testAccelerometerPresent();
    else if (testName == "testAccelerometerRead") out = testAccelerometerRead();
    else if (testName == "testAccelerometerScale") out = testAccelerometerScale();
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
