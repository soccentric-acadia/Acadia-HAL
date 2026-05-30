/**
 * @file analog_input_lib.cpp
 * @brief AnalogInputTesting implementation — IIO sysfs (/sys/bus/iio/devices/iio:device*, voltage channels)
 * @note HARDWARE-UNVERIFIED — on-target execution required to validate.
 */
#include "analog_input_lib.h"

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
AnalogInputTesting::AnalogInputTesting() = default;
AnalogInputTesting::~AnalogInputTesting() = default;

// ── TestBase interface ────────────────────────────────────────────────────────
std::vector<std::string> AnalogInputTesting::getAvailableTests() {
    return {"testPresence", "testChannelRead", "testResolution", "testSampleRate"};
}

std::vector<TestResult> AnalogInputTesting::runAllTests() {
    auto names = getAvailableTests();
    return runSelectedTests(names);
}

TestResult AnalogInputTesting::runSingleTest(const std::string& testName) {
    auto names = getAvailableTests();
    for (const auto& n : names) {
        if (n == testName) return runSelectedTests({testName})[0];
    }
    TestResult r;
    r.module_id        = "analog_input";
    r.test_id          = testName;
    r.name             = testName;
    r.timestamp        = std::chrono::system_clock::now();
    r.output.status    = Status::ERROR;
    r.output.severity  = Severity::ERROR;
    r.output.error_message = "unknown test: " + testName;
    return r;
}

// HARDWARE-UNVERIFIED: requires real analog_input hardware on target.
TestOutput AnalogInputTesting::testPresence() {
    TestOutput out;
    // Detect IIO voltage device in sysfs
    // Linux interface: IIO sysfs (/sys/bus/iio/devices/iio:device*, voltage channels)
    // TODO: implement against IIO sysfs (/sys/bus/iio/devices/iio:device*, voltage channels)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["device_found"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testPresence: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real analog_input hardware on target.
TestOutput AnalogInputTesting::testChannelRead() {
    TestOutput out;
    // Read raw voltage from channel 0
    // Linux interface: IIO sysfs (/sys/bus/iio/devices/iio:device*, voltage channels)
    // TODO: implement against IIO sysfs (/sys/bus/iio/devices/iio:device*, voltage channels)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["voltage_mv"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testChannelRead: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real analog_input hardware on target.
TestOutput AnalogInputTesting::testResolution() {
    TestOutput out;
    // Verify ADC bit resolution from sysfs
    // Linux interface: IIO sysfs (/sys/bus/iio/devices/iio:device*, voltage channels)
    // TODO: implement against IIO sysfs (/sys/bus/iio/devices/iio:device*, voltage channels)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["bits"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testResolution: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real analog_input hardware on target.
TestOutput AnalogInputTesting::testSampleRate() {
    TestOutput out;
    // Verify configured sample rate
    // Linux interface: IIO sysfs (/sys/bus/iio/devices/iio:device*, voltage channels)
    // TODO: implement against IIO sysfs (/sys/bus/iio/devices/iio:device*, voltage channels)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["sample_rate_hz"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testSampleRate: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}


