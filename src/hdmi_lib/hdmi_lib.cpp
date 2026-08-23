/**
 * @file hdmi_lib.cpp
 * @brief Implementation of HdmiTesting for Raspberry Pi 5 / Compute Module 5.
 * @details Checks HDMI DRM connectors (/sys/class/drm/card*-HDMI-*-1), connection
 *          status, available modes, and HDMI audio ALSA cards (/proc/asound/cards).
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */

#include "hdmi_lib.h"
#include "../platform_rpi5.h"
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <sstream>
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

HdmiTesting::HdmiTesting()
{
}
HdmiTesting::~HdmiTesting()
{
}

TestOutput HdmiTesting::testHdmiConnectorPresent()
{
    std::vector<std::string> data;
    data.push_back("=== HDMI Connector Presence ===");
    bool found = false;
    const char* drms[] = {RPI5_HDMI_DRM0, RPI5_HDMI_DRM1};
    for (const char* d : drms) {
        if (pathExists(d)) {
            data.push_back("[PASS] HDMI DRM connector present: " + std::string(d));
            found = true;
        }
    }
    if (!found) {
        for (const auto& c : listDir("/sys/class/drm")) {
            if (c.find("HDMI-") != std::string::npos) {
                data.push_back("[PASS] HDMI connector: " + c);
                found = true;
            }
        }
    }
    if (!found)
        data.push_back("[FAIL] No HDMI DRM connector found");
    {
        TestOutput _out;
        _out.status = found ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = found ? hal::Severity::INFO : hal::Severity::ERROR;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        return _out;
    }
}

TestOutput HdmiTesting::testHdmiConnected()
{
    std::vector<std::string> data;
    data.push_back("=== HDMI Monitor Connected ===");
    bool connected = false;
    for (const auto& c : listDir("/sys/class/drm")) {
        if (c.find("HDMI-") == std::string::npos)
            continue;
        std::string status = readSysfs(std::string("/sys/class/drm/") + c + "/status");
        data.push_back("[INFO] " + c + " status=" + status);
        if (status == "connected")
            connected = true;
    }
    if (!connected)
        data.push_back("[INFO] No HDMI monitor currently attached");
    {
        TestOutput _out;
        _out.status = connected ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = connected ? hal::Severity::INFO : hal::Severity::INFO;
        for (size_t _i = 0; _i < data.size(); ++_i)
            _out.reason["detail_" + std::to_string(_i)] = data[_i];
        _out.metrics["hdmi_connected"] = connected;
        return _out;
    }
}

TestOutput HdmiTesting::testHdmiResolution()
{
    std::vector<std::string> data;
    data.push_back("=== HDMI Resolution / Modes ===");
    bool read = false;
    for (const auto& c : listDir("/sys/class/drm")) {
        if (c.find("HDMI-") == std::string::npos)
            continue;
        std::string modes = readSysfs(std::string("/sys/class/drm/") + c + "/modes");
        if (!modes.empty()) {
            data.push_back("[PASS] " + c + " current mode: " + modes);
            read = true;
        }
    }
    if (!read) {
        std::string tv = runCommand("tvservice -s 2>/dev/null | head -1");
        if (!tv.empty()) {
            data.push_back("[PASS] tvservice: " + tv);
            read = true;
        } else {
            data.push_back("[INFO] No HDMI modes readable (no display)");
        }
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

TestOutput HdmiTesting::testHdmiAudio()
{
    std::vector<std::string> data;
    data.push_back("=== HDMI Audio (ALSA) ===");
    bool passed = false;
    std::string cards = runCommand("cat /proc/asound/cards 2>/dev/null | grep -i hdmi | head -4");
    if (!cards.empty()) {
        std::istringstream iss(cards);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty()) {
                data.push_back("[PASS] HDMI audio card: " + line);
                passed = true;
            }
        }
    } else {
        data.push_back("[INFO] No HDMI ALSA card (HDMI audio disabled or no display)");
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

std::vector<TestResult> HdmiTesting::runAllTests()
{
    std::vector<TestResult> results;
    const std::vector<std::pair<std::string, TestOutput (HdmiTesting::*)()>> tests = {
        {"testHdmiConnectorPresent", &HdmiTesting::testHdmiConnectorPresent},
        {"testHdmiConnected", &HdmiTesting::testHdmiConnected},
        {"testHdmiResolution", &HdmiTesting::testHdmiResolution},
        {"testHdmiAudio", &HdmiTesting::testHdmiAudio},
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

std::vector<std::string> HdmiTesting::getAvailableTests()
{
    return {"testHdmiConnectorPresent", "testHdmiConnected", "testHdmiResolution", "testHdmiAudio"};
}

TestResult HdmiTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput out;
    out.status = hal::Status::ERROR;
    out.severity = hal::Severity::ERROR;
    out.error_message = "test not dispatched";
    bool known = true;
    if (testName == "testHdmiConnectorPresent")
        out = testHdmiConnectorPresent();
    else if (testName == "testHdmiConnected")
        out = testHdmiConnected();
    else if (testName == "testHdmiResolution")
        out = testHdmiResolution();
    else if (testName == "testHdmiAudio")
        out = testHdmiAudio();
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
