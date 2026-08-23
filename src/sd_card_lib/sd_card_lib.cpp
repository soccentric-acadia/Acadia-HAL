/**
 * @file sd_card_lib.cpp
 * @brief Implementation of SdCardTesting for Raspberry Pi 5.
 * @details Checks the MMC host, SD card block device (/dev/mmcblk0), capacity,
 *          block access, and timing. Pi5 normally boots from microSD.
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */

#include "sd_card_lib.h"
#include "../platform_rpi5.h"
#include <dirent.h>
#include <fstream>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <chrono>
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

SdCardTesting::SdCardTesting() {}
SdCardTesting::~SdCardTesting() {}

TestOutput SdCardTesting::testSdCardPresent()
{
    std::vector<std::string> data;
    data.push_back("=== SD Card Presence ===");
    bool found = false;
    for (const auto& h : listDir(RPI5_SD_BLOCK_PATH)) {
        if (h.rfind("mmc", 0) != 0)
            continue;
        for (const auto& d : listDir(std::string(RPI5_SD_BLOCK_PATH) + "/" + h)) {
            if (d.find(':') == std::string::npos)
                continue;
            std::string type = readSysfs(std::string(RPI5_SD_BLOCK_PATH) + "/" + h + "/" + d + "/type");
            if (type == "SD") {
                std::string name = readSysfs(std::string(RPI5_SD_BLOCK_PATH) + "/" + h + "/" + d + "/name");
                data.push_back("[PASS] SD card: " + d + " name=" + name);
                found = true;
            }
        }
    }
    if (!found) {
        data.push_back("[FAIL] No SD card detected under /sys/class/mmc_host");
        if (pathExists(RPI5_MMCBLK_PATH)) {
            data.push_back("[INFO] mmcblk0 present; card likely removable/being checked");
            found = true;
        }
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

TestOutput SdCardTesting::testSdCardCapacity()
{
    std::vector<std::string> data;
    data.push_back("=== SD Card Capacity ===");
    bool read = false;
    if (pathExists(RPI5_MMCBLK_PATH)) {
        std::string size = readSysfs("/sys/class/block/mmcblk0/size");
        if (!size.empty()) {
            long long sectors = 0;
            try { sectors = std::stoll(size); } catch (...) {}
            double gb = sectors * 512.0 / (1024.0 * 1024.0 * 1024.0);
            data.push_back("[PASS] SD card size=" + size + " sectors (" + std::to_string(gb) + " GB)");
            read = true;
        }
    }
    if (!read)
        data.push_back("[INFO] SD capacity not readable");
    {
        TestOutput _out;
        _out.status = read ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = read ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        return _out;
    }
}

TestOutput SdCardTesting::testSdCardBlockAccess()
{
    std::vector<std::string> data;
    data.push_back("=== SD Card Block Access ===");
    bool passed = false;
    if (pathExists(RPI5_MMCBLK_PATH)) {
        int fd = open(RPI5_MMCBLK_PATH, O_RDONLY);
        if (fd >= 0) {
            char buf[512];
            ssize_t n = read(fd, buf, sizeof(buf));
            if (n > 0) {
                data.push_back("[PASS] Read " + std::to_string(n) + " bytes from SD card");
                passed = true;
            } else {
                data.push_back("[FAIL] SD read returned " + std::to_string(n) + " bytes");
            }
            close(fd);
        } else {
            data.push_back("[FAIL] Cannot open SD card: " + std::string(strerror(errno)));
        }
    } else {
        data.push_back("[INFO] SD card not present");
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

TestOutput SdCardTesting::testSdCardSpeed()
{
    std::vector<std::string> data;
    data.push_back("=== SD Card Speed (sustained read) ===");
    bool passed = false;
    double readMbps = 0.0;
    if (pathExists(RPI5_MMCBLK_PATH)) {
        int fd = open(RPI5_MMCBLK_PATH, O_RDONLY);
        if (fd >= 0) {
            std::vector<char> buf(1024 * 1024);
            auto t0 = std::chrono::steady_clock::now();
            ssize_t total = 0;
            ssize_t n;
            while ((n = read(fd, buf.data(), buf.size())) > 0 && total < 8LL * 1024 * 1024)
                total += n;
            auto t1 = std::chrono::steady_clock::now();
            close(fd);
            double mbps = (double)total / (1024.0 * 1024.0) /
                          std::chrono::duration<double>(t1 - t0).count();
            data.push_back("[PASS] Sustained read ~" + std::to_string(mbps) + " MB/s (" +
                           std::to_string(total / 1024) + " KB)");
            readMbps = mbps;
            passed = true;
        } else {
            data.push_back("[FAIL] Cannot open SD card for speed test");
        }
    } else {
        data.push_back("[INFO] SD card not present");
    }
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        if (readMbps > 0) _out.metrics["read_mbps"] = readMbps;
        return _out;
    }
}

std::vector<TestResult> SdCardTesting::runAllTests()
{
    std::vector<TestResult> results;
    const std::vector<std::pair<std::string, TestOutput (SdCardTesting::*)()>> tests = {
        {"testSdCardPresent", &SdCardTesting::testSdCardPresent},
        {"testSdCardCapacity", &SdCardTesting::testSdCardCapacity},
        {"testSdCardBlockAccess", &SdCardTesting::testSdCardBlockAccess},
        {"testSdCardSpeed", &SdCardTesting::testSdCardSpeed},
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

std::vector<std::string> SdCardTesting::getAvailableTests()
{
    return { "testSdCardPresent", "testSdCardCapacity", "testSdCardBlockAccess", "testSdCardSpeed" };
}

TestResult SdCardTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput out;
    out.status = hal::Status::ERROR;
    out.severity = hal::Severity::ERROR;
    out.error_message = "test not dispatched";
    bool known = true;
    if (testName == "testSdCardPresent") out = testSdCardPresent();
    else if (testName == "testSdCardCapacity") out = testSdCardCapacity();
    else if (testName == "testSdCardBlockAccess") out = testSdCardBlockAccess();
    else if (testName == "testSdCardSpeed") out = testSdCardSpeed();
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
