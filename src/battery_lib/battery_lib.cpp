/**
 * @file battery_lib.cpp
 * @brief Implementation of BatteryTesting for Raspberry Pi 5.
 * @details Reads battery status from /sys/class/power_supply (MXL7704 PMIC or a
 *          HAT battery). Handles the case where the Pi is USB powered (no battery).
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */

#include "battery_lib.h"
#include "../platform_rpi5.h"
#include <dirent.h>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>


// Safe numeric parsing: rejects empty/garbage input instead of throwing.
static bool batParseDouble(const std::string& s, double& out)
{
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    double v = strtod(s.c_str(), &end);
    if (end == s.c_str()) return false;
    out = v;
    return true;
}

static bool batParseLong(const std::string& s, long& out)
{
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    long v = strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || errno == ERANGE) return false;
    out = v;
    return true;
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

BatteryTesting::BatteryTesting() {}
BatteryTesting::~BatteryTesting() {}

TestOutput BatteryTesting::testBatteryPresent()
{
    std::vector<std::string> data;
    data.push_back("=== Battery Presence ===");
    bool found = false;
    for (const auto& s : listDir(RPI5_BATTERY_SUPPLY)) {
        std::string type = readSysfs(std::string(RPI5_BATTERY_SUPPLY) + "/" + s + "/type");
        if (type == "Battery") {
            data.push_back("[PASS] Battery supply found: " + s);
            found = true;
        } else {
            data.push_back("[INFO] Power supply: " + s + " (type=" + type + ")");
        }
    }
    if (!found)
        data.push_back("[INFO] No battery present - Pi5 is typically USB/mains powered");
    {
        TestOutput _out;
        _out.status = found ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = found ? hal::Severity::INFO : hal::Severity::INFO;
        StampDiagnostics(_out, data);
        _out.metrics["battery_present"] = found;
        return _out;
    }
}

TestOutput BatteryTesting::testBatteryChargeLevel()
{
    std::vector<std::string> data;
    data.push_back("=== Battery Charge Level ===");
    bool read = false;
    double capPercent = 0.0;
    for (const auto& s : listDir(RPI5_BATTERY_SUPPLY)) {
        std::string type = readSysfs(std::string(RPI5_BATTERY_SUPPLY) + "/" + s + "/type");
        if (type != "Battery")
            continue;
        std::string cap = readSysfs(std::string(RPI5_BATTERY_SUPPLY) + "/" + s + "/capacity");
        std::string enow = readSysfs(std::string(RPI5_BATTERY_SUPPLY) + "/" + s + "/energy_now");
        if (!cap.empty()) {
            data.push_back("[PASS] " + s + " capacity=" + cap + "%");
            try {
                capPercent = static_cast<double>(std::stoi(cap));
            } catch (...) {}
            read = true;
        } else if (!enow.empty()) {
            data.push_back("[INFO] " + s + " energy_now=" + enow + " uWh");
            read = true;
        }
    }
    if (!read)
        data.push_back("[INFO] No battery charge data available");
    {
        TestOutput _out;
        _out.status = read ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = read ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        if (capPercent > 0) _out.metrics["capacity_percent"] = capPercent;
        return _out;
    }
}

TestOutput BatteryTesting::testBatteryHealth()
{
    std::vector<std::string> data;
    data.push_back("=== Battery Health & Status ===");
    bool read = false;
    for (const auto& s : listDir(RPI5_BATTERY_SUPPLY)) {
        std::string type = readSysfs(std::string(RPI5_BATTERY_SUPPLY) + "/" + s + "/type");
        if (type != "Battery")
            continue;
        std::string health = readSysfs(std::string(RPI5_BATTERY_SUPPLY) + "/" + s + "/health");
        std::string status = readSysfs(std::string(RPI5_BATTERY_SUPPLY) + "/" + s + "/status");
        std::string cap = readSysfs(std::string(RPI5_BATTERY_SUPPLY) + "/" + s + "/capacity");
        data.push_back("[INFO] " + s + " status=" + status + " health=" + health + " capacity=" + cap + "%");
        read = true;
    }
    if (!read)
        data.push_back("[INFO] No battery health data available");
    {
        TestOutput _out;
        _out.status = read ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = read ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

TestOutput BatteryTesting::testBatteryVoltageCurrent()
{
    std::vector<std::string> data;
    data.push_back("=== Battery Voltage / Current ===");
    bool read = false;
    double voltage = 0.0, current = 0.0;
    for (const auto& s : listDir(RPI5_BATTERY_SUPPLY)) {
        std::string type = readSysfs(std::string(RPI5_BATTERY_SUPPLY) + "/" + s + "/type");
        if (type != "Battery")
            continue;
        std::string vnow = readSysfs(std::string(RPI5_BATTERY_SUPPLY) + "/" + s + "/voltage_now");
        std::string inow = readSysfs(std::string(RPI5_BATTERY_SUPPLY) + "/" + s + "/current_now");
        if (!vnow.empty()) {
            double vu = 0.0;
            if (!batParseDouble(vnow, vu) || vu < 0.0 || vu > 30e6) {
                data.push_back("[FAIL] " + s + " voltage value unparseable/out of range: '" + vnow + "' uV");
                TestOutput _out;
                _out.status = hal::Status::FAIL;
                _out.severity = hal::Severity::ERROR;
                StampDiagnostics(_out, data);
                return _out;
            }
            double v = vu / 1e6;
            data.push_back("[INFO] " + s + " voltage=" + vnow + " uV (" + std::to_string(v) + " V)");
            voltage = v;
            read = true;
        }
        if (!inow.empty()) {
            double iu = 0.0;
            if (!batParseDouble(inow, iu) || iu < -100e6 || iu > 100e6) {
                data.push_back("[FAIL] " + s + " current value unparseable/out of range: '" + inow + "' uA");
                TestOutput _out;
                _out.status = hal::Status::FAIL;
                _out.severity = hal::Severity::ERROR;
                StampDiagnostics(_out, data);
                return _out;
            }
            double i = iu / 1e6;
            data.push_back("[INFO] " + s + " current=" + inow + " uA (" + std::to_string(i) + " A)");
            current = i;
            read = true;
        }
    }
    if (!read)
        data.push_back("[INFO] No battery voltage/current data available");
    {
        TestOutput _out;
        _out.status = read ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = read ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        if (voltage > 0) _out.metrics["voltage_v"] = voltage;
        if (current > 0) _out.metrics["current_a"] = current;
        return _out;
    }
}

std::vector<TestResult> BatteryTesting::runAllTests()
{
    std::vector<TestResult> results;
    const std::vector<std::pair<std::string, TestOutput (BatteryTesting::*)()>> tests = {
        {"testBatteryPresent", &BatteryTesting::testBatteryPresent},
        {"testBatteryChargeLevel", &BatteryTesting::testBatteryChargeLevel},
        {"testBatteryHealth", &BatteryTesting::testBatteryHealth},
        {"testBatteryVoltageCurrent", &BatteryTesting::testBatteryVoltageCurrent},
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

std::vector<std::string> BatteryTesting::getAvailableTests()
{
    return { "testBatteryPresent", "testBatteryChargeLevel", "testBatteryHealth", "testBatteryVoltageCurrent" };
}

TestResult BatteryTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput out;
    out.status = hal::Status::ERROR;
    out.severity = hal::Severity::ERROR;
    out.error_message = "test not dispatched";
    bool known = true;
    if (testName == "testBatteryPresent") out = testBatteryPresent();
    else if (testName == "testBatteryChargeLevel") out = testBatteryChargeLevel();
    else if (testName == "testBatteryHealth") out = testBatteryHealth();
    else if (testName == "testBatteryVoltageCurrent") out = testBatteryVoltageCurrent();
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
