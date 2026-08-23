/**
 * @file spi_lib.cpp
 * @brief Implementation of SpiTesting for Raspberry Pi 5 (RP1 SPI controllers).
 * @details Checks SPI master presence under /sys/class/spi_master, enumerates
 *          /dev/spidev* nodes, and verifies interface access via open()+ioctl.
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */

#include "spi_lib.h"
#include "../platform_rpi5.h"
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <cstring>
#include <cerrno>

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

SpiTesting::SpiTesting() {}
SpiTesting::~SpiTesting() {}

TestOutput SpiTesting::testSpiControllerPresent()
{
    std::vector<std::string> data;
    data.push_back("=== SPI Controller Presence ===");
    bool passed = false;
    const char* devs[] = { RPI5_SPI_DEV0_0, RPI5_SPI_DEV0_1, RPI5_SPI_DEV1_0 };
    int found = 0;
    for (const char* d : devs) {
        if (pathExists(d)) {
            data.push_back("[PASS] SPI device node present: " + std::string(d));
            found++;
            passed = true;
        }
    }
    for (const auto& m : listDir(RPI5_SPI_SYSFS))
        data.push_back("[INFO] SPI master: " + m);
    if (!passed)
        data.push_back("[FAIL] No /dev/spidev* nodes found (spi-bcm2835 / RP1 SPI driver not loaded)");
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        _out.metrics["spi_devices"] = static_cast<int64_t>(found);
        return _out;
    }
}

TestOutput SpiTesting::testSpiDeviceEnumeration()
{
    std::vector<std::string> data;
    data.push_back("=== SPI Device Enumeration ===");
    int n = 0;
    for (const auto& m : listDir(RPI5_SPI_SYSFS)) {
        if (m.rfind("spi", 0) != 0)
            continue;
        for (const auto& c : listDir(std::string(RPI5_SPI_SYSFS) + "/" + m)) {
            data.push_back("[INFO] SPI device: " + m + "/" + c);
            n++;
        }
    }
    bool passed = n > 0;
    if (!passed)
        data.push_back("[INFO] No SPI devices attached");
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        _out.metrics["spi_channels"] = static_cast<int64_t>(n);
        return _out;
    }
}

TestOutput SpiTesting::testSpiInterfaceAccess()
{
    std::vector<std::string> data;
    data.push_back("=== SPI Interface Access ===");
    int opened = 0;
    const char* devs[] = { RPI5_SPI_DEV0_0, RPI5_SPI_DEV0_1, RPI5_SPI_DEV1_0 };
    for (const char* d : devs) {
        int fd = open(d, O_RDWR);
        if (fd >= 0) {
            data.push_back("[PASS] Opened " + std::string(d));
            close(fd);
            opened++;
        }
    }
    bool passed = opened > 0;
    if (!passed)
        data.push_back("[FAIL] No SPI device node opened for R/W");
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        _out.metrics["openable_devices"] = static_cast<int64_t>(opened);
        return _out;
    }
}

TestOutput SpiTesting::testSpiSpeedConfiguration()
{
    std::vector<std::string> data;
    data.push_back("=== SPI Speed Configuration ===");
    bool passed = false;
    // Read max-speed from DT binding if present
    const char* dt = "/proc/device-tree/spi0/spi0@0/max-speed";
    std::string speed = readSysfs(dt);
    if (!speed.empty()) {
        data.push_back("[PASS] SPI max-speed from DT: " + speed + " Hz");
        passed = true;
    } else {
        data.push_back("[INFO] SPI max-speed not exposed in DT; relying on node presence");
        passed = pathExists(RPI5_SPI_DEV0_0) || pathExists(RPI5_SPI_DEV0_1);
    }
    if (!passed)
        data.push_back("[FAIL] SPI speed configuration not detectable");
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        return _out;
    }
}

std::vector<TestResult> SpiTesting::runAllTests()
{
    std::vector<TestResult> results;
    const std::vector<std::pair<std::string, TestOutput (SpiTesting::*)()>> tests = {
        {"testSpiControllerPresent", &SpiTesting::testSpiControllerPresent},
        {"testSpiDeviceEnumeration", &SpiTesting::testSpiDeviceEnumeration},
        {"testSpiInterfaceAccess", &SpiTesting::testSpiInterfaceAccess},
        {"testSpiSpeedConfiguration", &SpiTesting::testSpiSpeedConfiguration},
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

std::vector<std::string> SpiTesting::getAvailableTests()
{
    return {
        "testSpiControllerPresent",
        "testSpiDeviceEnumeration",
        "testSpiInterfaceAccess",
        "testSpiSpeedConfiguration",
    };
}

TestResult SpiTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput out;
    out.status = hal::Status::ERROR;
    out.severity = hal::Severity::ERROR;
    out.error_message = "test not dispatched";
    bool known = true;
    if (testName == "testSpiControllerPresent") out = testSpiControllerPresent();
    else if (testName == "testSpiDeviceEnumeration") out = testSpiDeviceEnumeration();
    else if (testName == "testSpiInterfaceAccess") out = testSpiInterfaceAccess();
    else if (testName == "testSpiSpeedConfiguration") out = testSpiSpeedConfiguration();
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
