/**
 * @file accelerometer_lib.cpp
 * @brief AccelerometerTesting implementation — IIO sysfs (/sys/bus/iio/devices/iio:device*)
 * @note HARDWARE-UNVERIFIED — on-target execution required to validate.
 */
#include "accelerometer_lib.h"

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
AccelerometerTesting::AccelerometerTesting() = default;
AccelerometerTesting::~AccelerometerTesting() = default;

// ── TestBase interface ────────────────────────────────────────────────────────
std::vector<std::string> AccelerometerTesting::getAvailableTests() {
    return {"testPresence", "testAxisReadback", "testDataRate", "testSelfTest"};
}

std::vector<TestResult> AccelerometerTesting::runAllTests() {
    auto names = getAvailableTests();
    return runSelectedTests(names);
}

TestResult AccelerometerTesting::runSingleTest(const std::string& testName) {
    auto names = getAvailableTests();
    for (const auto& n : names) {
        if (n == testName) return runSelectedTests({testName})[0];
    }
    TestResult r;
    r.module_id        = "accelerometer";
    r.test_id          = testName;
    r.name             = testName;
    r.timestamp        = std::chrono::system_clock::now();
    r.output.status    = Status::ERROR;
    r.output.severity  = Severity::ERROR;
    r.output.error_message = "unknown test: " + testName;
    return r;
}

// HARDWARE-UNVERIFIED: requires real accelerometer hardware on target.
TestOutput AccelerometerTesting::testPresence() {
    TestOutput out;
    // Detect IIO accelerometer device in sysfs
    // Linux interface: IIO sysfs (/sys/bus/iio/devices/iio:device*)
    // TODO: implement against IIO sysfs (/sys/bus/iio/devices/iio:device*)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["device_found"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testPresence: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real accelerometer hardware on target.
TestOutput AccelerometerTesting::testAxisReadback() {
    TestOutput out;
    // Read X/Y/Z raw acceleration values
    // Linux interface: IIO sysfs (/sys/bus/iio/devices/iio:device*)
    // TODO: implement against IIO sysfs (/sys/bus/iio/devices/iio:device*)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["accel_x_raw"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testAxisReadback: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real accelerometer hardware on target.
TestOutput AccelerometerTesting::testDataRate() {
    TestOutput out;
    // Verify configured output data rate
    // Linux interface: IIO sysfs (/sys/bus/iio/devices/iio:device*)
    // TODO: implement against IIO sysfs (/sys/bus/iio/devices/iio:device*)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["sample_rate_hz"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testDataRate: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real accelerometer hardware on target.
TestOutput AccelerometerTesting::testSelfTest() {
    TestOutput out;
    // Run built-in self-test if supported
    // Linux interface: IIO sysfs (/sys/bus/iio/devices/iio:device*)
    // TODO: implement against IIO sysfs (/sys/bus/iio/devices/iio:device*)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["self_test_ok"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testSelfTest: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}


