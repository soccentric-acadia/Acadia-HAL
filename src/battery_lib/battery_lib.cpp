/**
 * @file battery_lib.cpp
 * @brief BatteryTesting implementation — power-supply sysfs (/sys/class/power_supply/battery/)
 * @note HARDWARE-UNVERIFIED — on-target execution required to validate.
 */
#include "battery_lib.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// ── helpers ──────────────────────────────────────────────────────────────────
[[maybe_unused]] static std::string readSysfs(const std::string& path) {
    std::ifstream f(path);
    std::string line;
    if (f.is_open()) std::getline(f, line);
    return line;
}

// ── constructor / destructor ─────────────────────────────────────────────────
BatteryTesting::BatteryTesting() = default;
BatteryTesting::~BatteryTesting() = default;

// ── TestBase interface ────────────────────────────────────────────────────────
std::vector<std::string> BatteryTesting::getAvailableTests() {
    return {"testPresence", "testChargeLevel", "testVoltage", "testStatus"};
}

std::vector<TestResult> BatteryTesting::runAllTests() {
    auto names = getAvailableTests();
    return runSelectedTests(names);
}

TestResult BatteryTesting::runSingleTest(const std::string& testName) {
    auto names = getAvailableTests();
    for (const auto& n : names) {
        if (n == testName) return runSelectedTests({testName})[0];
    }
    TestResult r;
    r.module_id        = "battery";
    r.test_id          = testName;
    r.name             = testName;
    r.timestamp        = std::chrono::system_clock::now();
    r.output.status    = Status::ERROR;
    r.output.severity  = Severity::ERROR;
    r.output.error_message = "unknown test: " + testName;
    return r;
}

// HARDWARE-UNVERIFIED: requires real battery hardware on target.
TestOutput BatteryTesting::testPresence() {
    TestOutput out;
    // Detect battery power-supply in sysfs
    // Linux interface: power-supply sysfs (/sys/class/power_supply/battery/)
    // TODO: implement against power-supply sysfs (/sys/class/power_supply/battery/)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["supply_found"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testPresence: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real battery hardware on target.
TestOutput BatteryTesting::testChargeLevel() {
    TestOutput out;
    // Read battery capacity percentage
    // Linux interface: power-supply sysfs (/sys/class/power_supply/battery/)
    // TODO: implement against power-supply sysfs (/sys/class/power_supply/battery/)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["capacity_pct"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testChargeLevel: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real battery hardware on target.
TestOutput BatteryTesting::testVoltage() {
    TestOutput out;
    // Read battery voltage in microvolts
    // Linux interface: power-supply sysfs (/sys/class/power_supply/battery/)
    // TODO: implement against power-supply sysfs (/sys/class/power_supply/battery/)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["voltage_uv"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testVoltage: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real battery hardware on target.
TestOutput BatteryTesting::testStatus() {
    TestOutput out;
    // Read charging/discharging status
    // Linux interface: power-supply sysfs (/sys/class/power_supply/battery/)
    // TODO: implement against power-supply sysfs (/sys/class/power_supply/battery/)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["status_string"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testStatus: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}


