/**
 * @file nvme_lib.cpp
 * @brief Implementation of NvmeTesting for Raspberry Pi 5 (optional NVMe via M.2).
 * @details Checks /dev/nvme*, reads SMART health from sysfs, reports capacity,
 *          and verifies block-device access. NVMe is optional on Pi5.
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */

#include "nvme_lib.h"
#include "../platform_rpi5.h"
#include <dirent.h>
#include <fstream>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <cstring>
#include <cerrno>

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

NvmeTesting::NvmeTesting() {}
NvmeTesting::~NvmeTesting() {}

TestOutput NvmeTesting::testNvmeDevicePresent()
{
    std::vector<std::string> data;
    data.push_back("=== NVMe Device Presence ===");
    bool passed = pathExists(RPI5_NVME_PATH);
    if (passed) {
        data.push_back("[PASS] NVMe block device present: " + std::string(RPI5_NVME_PATH));
    } else {
        data.push_back("[FAIL] " + std::string(RPI5_NVME_PATH) + " not present (no NVMe SSD in M.2 slot)");
        data.push_back("[INFO] NVMe is optional on Pi5 via M.2 HAT");
    }
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        _out.metrics["nvme_present"] = passed;
        return _out;
    }
}

TestOutput NvmeTesting::testNvmeHealth()
{
    std::vector<std::string> data;
    data.push_back("=== NVMe Health (SMART / sysfs) ===");
    bool read = false;
    // nvme0 dirs under /sys/class/nvme/
    for (const auto& n : listDir("/sys/class/nvme")) {
        if (n.rfind("nvme", 0) != 0)
            continue;
        std::string serial = readSysfs("/sys/class/nvme/" + n + "/serial");
        std::string model = readSysfs("/sys/class/nvme/" + n + "/model");
        std::string state = readSysfs("/sys/class/nvme/" + n + "/state");
        data.push_back("[INFO] " + n + ": model=" + model + " serial=" + serial + " state=" + state);
        read = true;
    }
    if (!read)
        data.push_back("[INFO] No NVMe controller in /sys/class/nvme");
    {
        TestOutput _out;
        _out.status = read ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = read ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        return _out;
    }
}

TestOutput NvmeTesting::testNvmeCapacity()
{
    std::vector<std::string> data;
    data.push_back("=== NVMe Capacity ===");
    bool read = false;
    std::string size = readSysfs("/sys/class/block/nvme0n1/size");
    if (!size.empty()) {
        long long sectors = 0;
        try { sectors = std::stoll(size); } catch (...) {}
        double gb = sectors * 512.0 / (1024.0 * 1024.0 * 1024.0);
        data.push_back("[PASS] NVMe size=" + size + " sectors (" + std::to_string(gb) + " GB)");
        read = true;
    } else {
        data.push_back("[INFO] NVMe capacity not readable");
    }
    {
        TestOutput _out;
        _out.status = read ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = read ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        return _out;
    }
}

TestOutput NvmeTesting::testNvmeBlockAccess()
{
    std::vector<std::string> data;
    data.push_back("=== NVMe Block Device Access ===");
    bool passed = false;
    if (pathExists(RPI5_NVME_PATH)) {
        int fd = open(RPI5_NVME_PATH, O_RDONLY);
        if (fd >= 0) {
            char buf[512];
            ssize_t n = read(fd, buf, sizeof(buf));
            if (n > 0) {
                data.push_back("[PASS] Read " + std::to_string(n) + " bytes from NVMe (readable)");
                passed = true;
            } else {
                data.push_back("[FAIL] NVMe read returned " + std::to_string(n) + " bytes");
            }
            close(fd);
        } else {
            data.push_back("[FAIL] Cannot open NVMe: " + std::string(strerror(errno)));
        }
    } else {
        data.push_back("[INFO] NVMe not present; block access not applicable");
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

std::vector<TestResult> NvmeTesting::runAllTests()
{
    std::vector<TestResult> results;
    const std::vector<std::pair<std::string, TestOutput (NvmeTesting::*)()>> tests = {
        {"testNvmeDevicePresent", &NvmeTesting::testNvmeDevicePresent},
        {"testNvmeHealth", &NvmeTesting::testNvmeHealth},
        {"testNvmeCapacity", &NvmeTesting::testNvmeCapacity},
        {"testNvmeBlockAccess", &NvmeTesting::testNvmeBlockAccess},
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

std::vector<std::string> NvmeTesting::getAvailableTests()
{
    return { "testNvmeDevicePresent", "testNvmeHealth", "testNvmeCapacity", "testNvmeBlockAccess" };
}

TestResult NvmeTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput out;
    out.status = hal::Status::ERROR;
    out.severity = hal::Severity::ERROR;
    out.error_message = "test not dispatched";
    bool known = true;
    if (testName == "testNvmeDevicePresent") out = testNvmeDevicePresent();
    else if (testName == "testNvmeHealth") out = testNvmeHealth();
    else if (testName == "testNvmeCapacity") out = testNvmeCapacity();
    else if (testName == "testNvmeBlockAccess") out = testNvmeBlockAccess();
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
