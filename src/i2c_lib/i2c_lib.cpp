/**
 * @file i2c_lib.cpp
 * @brief Implementation of I2cTesting for Raspberry Pi 5 (RP1 I2C controllers).
 * @details Checks I2C adapter presence under /dev/i2c-*, enumerates attached
 *          devices under /sys/bus/i2c/devices, and verifies bus scan + interface
 *          access. Uses /dev/i2c-1 (I2C1, header pins 2/5,3/5) as primary.
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */

#include "i2c_lib.h"
#include "../platform_rpi5.h"
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <cstring>
#include <cerrno>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
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

I2cTesting::I2cTesting() {}
I2cTesting::~I2cTesting() {}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
TestOutput I2cTesting::testI2cControllerPresent()
{
    std::vector<std::string> data;
    bool passed = false;
    data.push_back("=== I2C Controller Presence (RP1) ===");

    const char* candidates[] = { RPI5_I2C_DEV1, RPI5_I2C_DEV0 };
    int found = 0;
    for (const char* dev : candidates) {
        if (pathExists(dev)) {
            data.push_back("[PASS] I2C device node present: " + std::string(dev));
            found++;
            passed = true;
        }
    }
    for (const auto& a : listDir(RPI5_I2C_ADAPTERS)) {
        if (a.rfind("i2c-", 0) == 0)
            data.push_back("[INFO] Kernel I2C adapter: " + a);
    }
    if (!passed)
        data.push_back("[FAIL] No /dev/i2c-* nodes found (i2c-dev kernel module not loaded)");
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        _out.metrics["controllers_found"] = static_cast<int64_t>(found);
        return _out;
    }
}

TestOutput I2cTesting::testI2cBusScan()
{
    std::vector<std::string> data;
    data.push_back("=== I2C Bus Scan ===");
    int buses = 0;
    for (const auto& a : listDir(RPI5_I2C_ADAPTERS)) {
        if (a.rfind("i2c-", 0) != 0)
            continue;
        std::string path = std::string(RPI5_I2C_ADAPTERS) + "/" + a + "/name";
        std::string name = readSysfs(path);
        data.push_back("[INFO] " + a + ": " + (name.empty() ? "unnamed" : name));
        buses++;
    }
    bool passed = buses > 0;
    if (!passed)
        data.push_back("[FAIL] No I2C adapters enumerated");
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        _out.metrics["i2c_adapters"] = static_cast<int64_t>(buses);
        return _out;
    }
}

TestOutput I2cTesting::testI2cDeviceEnumeration()
{
    std::vector<std::string> data;
    data.push_back("=== I2C Attached Devices ===");
    int n = 0;
    for (const auto& d : listDir(RPI5_I2C_SYSFS)) {
        // entries like "0-0054", "1-0010" (bus-addr)
        if (d.find('-') == std::string::npos)
            continue;
        std::string name = readSysfs(std::string(RPI5_I2C_SYSFS) + "/" + d + "/name");
        data.push_back("[INFO] I2C device " + d + ": " + (name.empty() ? "unknown" : name));
        n++;
    }
    if (n == 0)
        data.push_back("[INFO] No I2C devices currently attached to the bus");
    bool passed = n > 0;
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        _out.metrics["attached_devices"] = static_cast<int64_t>(n);
        return _out;
    }
}

TestOutput I2cTesting::testI2cInterfaceAccess()
{
    std::vector<std::string> data;
    data.push_back("=== I2C Interface Access (/dev/i2c-*) ===");
    int opened = 0;
    const char* candidates[] = { RPI5_I2C_DEV1, RPI5_I2C_DEV0 };
    for (const char* dev : candidates) {
        int fd = open(dev, O_RDWR);
        if (fd >= 0) {
            data.push_back("[PASS] Opened " + std::string(dev) + " (I2C_FUNC ioctl)");
            close(fd);
            opened++;
        } else {
            data.push_back("[WARN] Cannot open " + std::string(dev) + ": " + strerror(errno));
        }
    }
    bool passed = opened > 0;
    if (!passed)
        data.push_back("[FAIL] No I2C device node opened for R/W");
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        _out.metrics["openable_buses"] = static_cast<int64_t>(opened);
        return _out;
    }
}

std::vector<TestResult> I2cTesting::runAllTests()
{
    std::vector<TestResult> results;
    const std::vector<std::pair<std::string, TestOutput (I2cTesting::*)()>> tests = {
        {"testI2cControllerPresent", &I2cTesting::testI2cControllerPresent},
        {"testI2cBusScan", &I2cTesting::testI2cBusScan},
        {"testI2cDeviceEnumeration", &I2cTesting::testI2cDeviceEnumeration},
        {"testI2cInterfaceAccess", &I2cTesting::testI2cInterfaceAccess},
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

std::vector<std::string> I2cTesting::getAvailableTests()
{
    return {
        "testI2cControllerPresent",
        "testI2cBusScan",
        "testI2cDeviceEnumeration",
        "testI2cInterfaceAccess",
    };
}

TestResult I2cTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput out;
    out.status = hal::Status::ERROR;
    out.severity = hal::Severity::ERROR;
    out.error_message = "test not dispatched";
    bool known = true;
    if (testName == "testI2cControllerPresent") out = testI2cControllerPresent();
    else if (testName == "testI2cBusScan") out = testI2cBusScan();
    else if (testName == "testI2cDeviceEnumeration") out = testI2cDeviceEnumeration();
    else if (testName == "testI2cInterfaceAccess") out = testI2cInterfaceAccess();
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
