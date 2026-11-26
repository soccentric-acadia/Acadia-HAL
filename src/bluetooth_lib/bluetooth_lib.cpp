/**
 * @file bluetooth_lib.cpp
 * @brief Implementation of BluetoothTesting for Raspberry Pi 5.
 * @details Checks for the built-in BCM43455 Bluetooth controller (/dev/hci0 or
 *          /sys/class/bluetooth), its power state, and HCI availability.
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */

#include "bluetooth_lib.h"
#include "../platform_rpi5.h"
#include <dirent.h>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <cstdio>

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

BluetoothTesting::BluetoothTesting() {}
BluetoothTesting::~BluetoothTesting() {}

TestOutput BluetoothTesting::testBluetoothControllerPresent()
{
    std::vector<std::string> data;
    data.push_back("=== Bluetooth Controller Presence ===");
    bool found = pathExists(RPI5_BT_HCI0);
    if (found) {
        data.push_back("[PASS] Bluetooth HCI device present: " + std::string(RPI5_BT_HCI0));
    } else {
        for (const auto& d : listDir("/dev/hci")) {
            data.push_back("[INFO] HCI device node: /dev/hci/" + d);
            found = true;
        }
    }
    for (const auto& a : listDir(RPI5_BT_SYSFS)) {
        if (a.rfind("hci", 0) == 0) {
            std::string addr = readSysfs(std::string(RPI5_BT_SYSFS) + "/" + a + "/address");
            data.push_back("[PASS] Bluetooth adapter: " + a + " addr=" + addr);
            found = true;
        }
    }
    if (!found)
        data.push_back("[FAIL] No Bluetooth controller found");
    {
        TestOutput _out;
        _out.status = found ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = found ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

TestOutput BluetoothTesting::testBluetoothPowered()
{
    std::vector<std::string> data;
    data.push_back("=== Bluetooth Power State ===");
    bool up = false;
    for (const auto& a : listDir(RPI5_BT_SYSFS)) {
        if (a.rfind("hci", 0) != 0)
            continue;
        std::string power = readSysfs(std::string(RPI5_BT_SYSFS) + "/" + a + "/power/runtime_status");
        data.push_back("[INFO] " + a + " power=" + power);
        up = true;
    }
    if (!up) {
        std::string hcistate = runCommand("hciconfig 2>&1 | grep -i 'UP RUNNING' | head -1");
        if (!hcistate.empty()) {
            data.push_back("[PASS] Bluetooth is UP RUNNING");
            up = true;
        }
    }
    if (!up) {
        std::string hcinfo = runCommand("hciconfig 2>&1 | head -3");
        if (!hcinfo.empty()) {
            data.push_back("[INFO] hciconfig: " + hcinfo);
            up = true;
        }
    }
    if (!up)
        data.push_back("[FAIL] Bluetooth adapter not powered");
    {
        TestOutput _out;
        _out.status = up ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = up ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

TestOutput BluetoothTesting::testBluetoothHciAvailable()
{
    std::vector<std::string> data;
    data.push_back("=== Bluetooth HCI Availability ===");
    bool passed = false;
    if (pathExists(RPI5_BT_HCI0)) {
        data.push_back("[PASS] HCI device openable: " + std::string(RPI5_BT_HCI0));
        passed = true;
    }
    for (const auto& a : listDir(RPI5_BT_SYSFS)) {
        if (a.rfind("hci", 0) == 0) {
            data.push_back("[PASS] HCI adapter present: " + a);
            passed = true;
        }
    }
    if (!passed)
        data.push_back("[FAIL] No HCI interface available (bluetoothd/btattach not started?)");
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

std::vector<TestResult> BluetoothTesting::runAllTests()
{
    std::vector<TestResult> results;
    const std::vector<std::pair<std::string, TestOutput (BluetoothTesting::*)()>> tests = {
        {"testBluetoothControllerPresent", &BluetoothTesting::testBluetoothControllerPresent},
        {"testBluetoothPowered", &BluetoothTesting::testBluetoothPowered},
        {"testBluetoothHciAvailable", &BluetoothTesting::testBluetoothHciAvailable},
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

std::vector<std::string> BluetoothTesting::getAvailableTests()
{
    return { "testBluetoothControllerPresent", "testBluetoothPowered", "testBluetoothHciAvailable" };
}

TestResult BluetoothTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput out;
    out.status = hal::Status::ERROR;
    out.severity = hal::Severity::ERROR;
    out.error_message = "test not dispatched";
    bool known = true;
    if (testName == "testBluetoothControllerPresent") out = testBluetoothControllerPresent();
    else if (testName == "testBluetoothPowered") out = testBluetoothPowered();
    else if (testName == "testBluetoothHciAvailable") out = testBluetoothHciAvailable();
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
