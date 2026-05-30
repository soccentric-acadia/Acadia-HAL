/**
 * @file pwm_output_lib.cpp
 * @brief PwmOutputTesting implementation — PWM sysfs (/sys/class/pwm/pwmchip0/)
 * @note HARDWARE-UNVERIFIED — on-target execution required to validate.
 */
#include "pwm_output_lib.h"

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
PwmOutputTesting::PwmOutputTesting() = default;
PwmOutputTesting::~PwmOutputTesting() = default;

// ── TestBase interface ────────────────────────────────────────────────────────
std::vector<std::string> PwmOutputTesting::getAvailableTests() {
    return {"testPresence", "testExport", "testPeriod", "testDutyCycle"};
}

std::vector<TestResult> PwmOutputTesting::runAllTests() {
    auto names = getAvailableTests();
    return runSelectedTests(names);
}

TestResult PwmOutputTesting::runSingleTest(const std::string& testName) {
    auto names = getAvailableTests();
    for (const auto& n : names) {
        if (n == testName) return runSelectedTests({testName})[0];
    }
    TestResult r;
    r.module_id        = "pwm_output";
    r.test_id          = testName;
    r.name             = testName;
    r.timestamp        = std::chrono::system_clock::now();
    r.output.status    = Status::ERROR;
    r.output.severity  = Severity::ERROR;
    r.output.error_message = "unknown test: " + testName;
    return r;
}

// HARDWARE-UNVERIFIED: requires real pwm_output hardware on target.
TestOutput PwmOutputTesting::testPresence() {
    TestOutput out;
    // Detect PWM chip in sysfs
    // Linux interface: PWM sysfs (/sys/class/pwm/pwmchip0/)
    // TODO: implement against PWM sysfs (/sys/class/pwm/pwmchip0/)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["chip_found"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testPresence: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real pwm_output hardware on target.
TestOutput PwmOutputTesting::testExport() {
    TestOutput out;
    // Export PWM channel via sysfs
    // Linux interface: PWM sysfs (/sys/class/pwm/pwmchip0/)
    // TODO: implement against PWM sysfs (/sys/class/pwm/pwmchip0/)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["export_ok"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testExport: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real pwm_output hardware on target.
TestOutput PwmOutputTesting::testPeriod() {
    TestOutput out;
    // Set and verify PWM period
    // Linux interface: PWM sysfs (/sys/class/pwm/pwmchip0/)
    // TODO: implement against PWM sysfs (/sys/class/pwm/pwmchip0/)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["period_ns"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testPeriod: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real pwm_output hardware on target.
TestOutput PwmOutputTesting::testDutyCycle() {
    TestOutput out;
    // Set and verify duty cycle
    // Linux interface: PWM sysfs (/sys/class/pwm/pwmchip0/)
    // TODO: implement against PWM sysfs (/sys/class/pwm/pwmchip0/)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["duty_cycle_ns"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testDutyCycle: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}


