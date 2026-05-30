/**
 * @file rtc_lib.cpp
 * @brief RtcTesting implementation — RTC ioctl (/dev/rtc0, RTC_RD_TIME / RTC_SET_TIME)
 * @note HARDWARE-UNVERIFIED — on-target execution required to validate.
 */
#include "rtc_lib.h"

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
RtcTesting::RtcTesting() = default;
RtcTesting::~RtcTesting() = default;

// ── TestBase interface ────────────────────────────────────────────────────────
std::vector<std::string> RtcTesting::getAvailableTests() {
    return {"testPresence", "testReadTime", "testAlarmRead", "testAccuracy"};
}

std::vector<TestResult> RtcTesting::runAllTests() {
    auto names = getAvailableTests();
    return runSelectedTests(names);
}

TestResult RtcTesting::runSingleTest(const std::string& testName) {
    auto names = getAvailableTests();
    for (const auto& n : names) {
        if (n == testName) return runSelectedTests({testName})[0];
    }
    TestResult r;
    r.module_id        = "rtc";
    r.test_id          = testName;
    r.name             = testName;
    r.timestamp        = std::chrono::system_clock::now();
    r.output.status    = Status::ERROR;
    r.output.severity  = Severity::ERROR;
    r.output.error_message = "unknown test: " + testName;
    return r;
}

// HARDWARE-UNVERIFIED: requires real rtc hardware on target.
TestOutput RtcTesting::testPresence() {
    TestOutput out;
    // Detect /dev/rtc0 device node
    // Linux interface: RTC ioctl (/dev/rtc0, RTC_RD_TIME / RTC_SET_TIME)
    // TODO: implement against RTC ioctl (/dev/rtc0, RTC_RD_TIME / RTC_SET_TIME)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["device_found"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testPresence: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real rtc hardware on target.
TestOutput RtcTesting::testReadTime() {
    TestOutput out;
    // Read current RTC time via RTC_RD_TIME ioctl
    // Linux interface: RTC ioctl (/dev/rtc0, RTC_RD_TIME / RTC_SET_TIME)
    // TODO: implement against RTC ioctl (/dev/rtc0, RTC_RD_TIME / RTC_SET_TIME)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["epoch_seconds"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testReadTime: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real rtc hardware on target.
TestOutput RtcTesting::testAlarmRead() {
    TestOutput out;
    // Read alarm state via RTC_RD_ALARM ioctl
    // Linux interface: RTC ioctl (/dev/rtc0, RTC_RD_TIME / RTC_SET_TIME)
    // TODO: implement against RTC ioctl (/dev/rtc0, RTC_RD_TIME / RTC_SET_TIME)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["alarm_enabled"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testAlarmRead: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real rtc hardware on target.
TestOutput RtcTesting::testAccuracy() {
    TestOutput out;
    // Compare RTC time against system clock
    // Linux interface: RTC ioctl (/dev/rtc0, RTC_RD_TIME / RTC_SET_TIME)
    // TODO: implement against RTC ioctl (/dev/rtc0, RTC_RD_TIME / RTC_SET_TIME)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["drift_ppm"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testAccuracy: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}


