/**
 * @file emmc_lib.cpp
 * @brief Implementation of EmmcTesting for Raspberry Pi 5.
 * @details Checks MMC host controllers and block devices (/dev/mmcblk*). Note:
 *          standard Pi5 boots from microSD; a true eMMC requires a carrier board.
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */

#include "emmc_lib.h"
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

EmmcTesting::EmmcTesting() {}
EmmcTesting::~EmmcTesting() {}

TestOutput EmmcTesting::testEmmcDevicePresent()
{
    std::vector<std::string> data;
    data.push_back("=== eMMC / MMC Device Presence ===");
    bool found = false;
    for (const auto& h : listDir(RPI5_EMMC_BLOCK_PATH)) {
        if (h.rfind("mmc", 0) != 0)
            continue;
        // each mmc host dir contains mmc<num>:0001 device dirs
        for (const auto& d : listDir(std::string(RPI5_EMMC_BLOCK_PATH) + "/" + h)) {
            if (d.find(':') == std::string::npos)
                continue;
            std::string name = readSysfs(std::string(RPI5_EMMC_BLOCK_PATH) + "/" + h + "/" + d + "/name");
            data.push_back("[INFO] MMC device " + d + " name=" + name);
            found = true;
        }
    }
    if (!found)
        data.push_back("[INFO] No MMC/eMMC devices found (Pi5 usually boots from microSD)");
    {
        TestOutput _out;
        _out.status = found ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = found ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

TestOutput EmmcTesting::testEmmcCapacity()
{
    std::vector<std::string> data;
    data.push_back("=== MMC Capacity ===");
    bool read = false;
    for (const auto& blk : listDir("/sys/class/block")) {
        if (blk.rfind("mmcblk", 0) != 0)
            continue;
        if (blk.find('p') != std::string::npos)
            continue; // skip partitions
        std::string size = readSysfs("/sys/class/block/" + blk + "/size");
        if (!size.empty()) {
            long long sectors = 0;
            try { sectors = std::stoll(size); } catch (...) {}
            double gb = sectors * 512.0 / (1024.0 * 1024.0 * 1024.0);
            data.push_back("[PASS] " + blk + " capacity=" + std::to_string(gb) + " GB");
            read = true;
        }
    }
    if (!read)
        data.push_back("[INFO] No MMC block devices for capacity check");
    {
        TestOutput _out;
        _out.status = read ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = read ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

TestOutput EmmcTesting::testEmmcBlockAccess()
{
    std::vector<std::string> data;
    data.push_back("=== MMC Block Access ===");
    bool passed = false;
    if (pathExists(RPI5_MMCBLK_PATH)) {
        int fd = open(RPI5_MMCBLK_PATH, O_RDONLY);
        if (fd >= 0) {
            char buf[512];
            ssize_t n = read(fd, buf, sizeof(buf));
            if (n > 0) {
                data.push_back("[PASS] Read " + std::to_string(n) + " bytes from " + std::string(RPI5_MMCBLK_PATH));
                passed = true;
            } else {
                data.push_back("[FAIL] MMC read returned " + std::to_string(n) + " bytes");
            }
            close(fd);
        } else {
            data.push_back("[FAIL] Cannot open MMC: " + std::string(strerror(errno)));
        }
    } else {
        data.push_back("[INFO] MMC block device not present");
    }
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

TestOutput EmmcTesting::testEmmcType()
{
    std::vector<std::string> data;
    data.push_back("=== MMC Media Type ===");
    bool read = false;
    for (const auto& h : listDir(RPI5_EMMC_BLOCK_PATH)) {
        if (h.rfind("mmc", 0) != 0)
            continue;
        for (const auto& d : listDir(std::string(RPI5_EMMC_BLOCK_PATH) + "/" + h)) {
            if (d.find(':') == std::string::npos)
                continue;
            std::string type = readSysfs(std::string(RPI5_EMMC_BLOCK_PATH) + "/" + h + "/" + d + "/type");
            std::string rev = readSysfs(std::string(RPI5_EMMC_BLOCK_PATH) + "/" + h + "/" + d + "/csd");
            data.push_back("[INFO] " + d + " type=" + type);
            read = true;
        }
    }
    if (!read)
        data.push_back("[INFO] No MMC type info available");
    {
        TestOutput _out;
        _out.status = read ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = read ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

std::vector<TestResult> EmmcTesting::runAllTests()
{
    std::vector<TestResult> results;
    const std::vector<std::pair<std::string, TestOutput (EmmcTesting::*)()>> tests = {
        {"testEmmcDevicePresent", &EmmcTesting::testEmmcDevicePresent},
        {"testEmmcCapacity", &EmmcTesting::testEmmcCapacity},
        {"testEmmcBlockAccess", &EmmcTesting::testEmmcBlockAccess},
        {"testEmmcType", &EmmcTesting::testEmmcType},
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

std::vector<std::string> EmmcTesting::getAvailableTests()
{
    return { "testEmmcDevicePresent", "testEmmcCapacity", "testEmmcBlockAccess", "testEmmcType" };
}

TestResult EmmcTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput out;
    out.status = hal::Status::ERROR;
    out.severity = hal::Severity::ERROR;
    out.error_message = "test not dispatched";
    bool known = true;
    if (testName == "testEmmcDevicePresent") out = testEmmcDevicePresent();
    else if (testName == "testEmmcCapacity") out = testEmmcCapacity();
    else if (testName == "testEmmcBlockAccess") out = testEmmcBlockAccess();
    else if (testName == "testEmmcType") out = testEmmcType();
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
