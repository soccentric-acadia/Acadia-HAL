/**
 * @file wifi_lib.cpp
 * @brief Implementation of WifiTesting for Raspberry Pi 5 / Compute Module 5.
 * @details Checks the built-in BCM2805 802.11ac Wi-Fi radio (wlan0) via sysfs
 *          and /proc/net/wireless: interface presence, link state, signal, and
 *          AP scan (using `iw` when available).
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */

#include "wifi_lib.h"
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

WifiTesting::WifiTesting()
{
}
WifiTesting::~WifiTesting()
{
}

TestOutput WifiTesting::testWifiInterfacePresent()
{
    std::vector<std::string> data;
    data.push_back("=== Wi-Fi Interface Presence ===");
    bool found = false;
    std::string base = std::string(RPI5_NET_SYSFS) + "/" + RPI5_WLAN_IFACE;
    if (pathExists(base)) {
        std::string addr = readSysfs(base + "/address");
        data.push_back("[PASS] Wi-Fi interface " + std::string(RPI5_WLAN_IFACE) + " present (MAC " + addr + ")");
        found = true;
    } else {
        data.push_back("[FAIL] No wlan0 interface (Wi-Fi disabled in config.txt / not on carrier)");
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

TestOutput WifiTesting::testWifiLinkUp()
{
    std::vector<std::string> data;
    data.push_back("=== Wi-Fi Link State ===");
    bool passed = false;
    std::string base = std::string(RPI5_NET_SYSFS) + "/" + RPI5_WLAN_IFACE;
    std::string operstate = readSysfs(base + "/operstate");
    if (!operstate.empty()) {
        data.push_back("[INFO] wlan0 operstate=" + operstate);
        passed = (operstate == "up" || operstate == "unknown");
        if (passed)
            data.push_back("[PASS] Wi-Fi link is up");
    } else {
        data.push_back("[FAIL] wlan0 operstate not readable");
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

TestOutput WifiTesting::testWifiSignalStrength()
{
    std::vector<std::string> data;
    data.push_back("=== Wi-Fi Signal Strength ===");
    bool read = false;
    // /proc/net/wireless reports signal (dBm) for each wireless interface
    std::ifstream w(RPI5_WIRELESS_PROC);
    std::string line;
    while (std::getline(w, line)) {
        if (line.find("wlan0") != std::string::npos) {
            data.push_back("[INFO] " + line);
            read = true;
        }
    }
    if (!read) {
        std::string iw = runCommand("iw dev wlan0 link 2>/dev/null | grep -i signal | head -1");
        if (!iw.empty()) {
            data.push_back("[PASS] " + iw);
            read = true;
        } else {
            data.push_back("[INFO] Signal not reportable (Wi-Fi down or not connected)");
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

TestOutput WifiTesting::testWifiScan()
{
    std::vector<std::string> data;
    data.push_back("=== Wi-Fi Scan ===");
    bool passed = false;
    std::string scan = runCommand("iw dev wlan0 scan 2>/dev/null | grep -E 'SSID:|signal:' | head -12");
    if (!scan.empty()) {
        std::istringstream iss(scan);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty()) {
                data.push_back("[PASS] " + line);
                passed = true;
            }
        }
    } else {
        data.push_back("[INFO] Scan unavailable (no perms or Wi-Fi not up)");
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

std::vector<TestResult> WifiTesting::runAllTests()
{
    std::vector<TestResult> results;
    const std::vector<std::pair<std::string, TestOutput (WifiTesting::*)()>> tests = {
        {"testWifiInterfacePresent", &WifiTesting::testWifiInterfacePresent},
        {"testWifiLinkUp", &WifiTesting::testWifiLinkUp},
        {"testWifiSignalStrength", &WifiTesting::testWifiSignalStrength},
        {"testWifiScan", &WifiTesting::testWifiScan},
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

std::vector<std::string> WifiTesting::getAvailableTests()
{
    return {"testWifiInterfacePresent", "testWifiLinkUp", "testWifiSignalStrength", "testWifiScan"};
}

TestResult WifiTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput out;
    out.status = hal::Status::ERROR;
    out.severity = hal::Severity::ERROR;
    out.error_message = "test not dispatched";
    bool known = true;
    if (testName == "testWifiInterfacePresent")
        out = testWifiInterfacePresent();
    else if (testName == "testWifiLinkUp")
        out = testWifiLinkUp();
    else if (testName == "testWifiSignalStrength")
        out = testWifiSignalStrength();
    else if (testName == "testWifiScan")
        out = testWifiScan();
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
