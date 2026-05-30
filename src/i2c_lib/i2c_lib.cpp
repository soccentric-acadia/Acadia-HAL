/**
 * @file i2c_lib.cpp
 * @brief I2cTesting implementation — i2c-dev (/dev/i2c-*, I2C_SMBUS ioctl)
 * @note HARDWARE-UNVERIFIED — on-target execution required to validate.
 */
#include "i2c_lib.h"

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
I2cTesting::I2cTesting() = default;
I2cTesting::~I2cTesting() = default;

// ── TestBase interface ────────────────────────────────────────────────────────
std::vector<std::string> I2cTesting::getAvailableTests() {
    return {"testPresence", "testBusScan", "testClockStretch", "testReadWrite"};
}

std::vector<TestResult> I2cTesting::runAllTests() {
    auto names = getAvailableTests();
    return runSelectedTests(names);
}

TestResult I2cTesting::runSingleTest(const std::string& testName) {
    auto names = getAvailableTests();
    for (const auto& n : names) {
        if (n == testName) return runSelectedTests({testName})[0];
    }
    TestResult r;
    r.module_id        = "i2c";
    r.test_id          = testName;
    r.name             = testName;
    r.timestamp        = std::chrono::system_clock::now();
    r.output.status    = Status::ERROR;
    r.output.severity  = Severity::ERROR;
    r.output.error_message = "unknown test: " + testName;
    return r;
}

// HARDWARE-UNVERIFIED: requires real i2c hardware on target.
TestOutput I2cTesting::testPresence() {
    TestOutput out;
    // Detect I2C bus device node
    // Linux interface: i2c-dev (/dev/i2c-*, I2C_SMBUS ioctl)
    // TODO: implement against i2c-dev (/dev/i2c-*, I2C_SMBUS ioctl)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["bus_found"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testPresence: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real i2c hardware on target.
TestOutput I2cTesting::testBusScan() {
    TestOutput out;
    // Count ACKing devices on the bus
    // Linux interface: i2c-dev (/dev/i2c-*, I2C_SMBUS ioctl)
    // TODO: implement against i2c-dev (/dev/i2c-*, I2C_SMBUS ioctl)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["devices_found"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testBusScan: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real i2c hardware on target.
TestOutput I2cTesting::testClockStretch() {
    TestOutput out;
    // Verify clock-stretch capability (i2c-dev ioctl)
    // Linux interface: i2c-dev (/dev/i2c-*, I2C_SMBUS ioctl)
    // TODO: implement against i2c-dev (/dev/i2c-*, I2C_SMBUS ioctl)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["stretch_ok"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testClockStretch: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real i2c hardware on target.
TestOutput I2cTesting::testReadWrite() {
    TestOutput out;
    // Perform a register read/write round-trip
    // Linux interface: i2c-dev (/dev/i2c-*, I2C_SMBUS ioctl)
    // TODO: implement against i2c-dev (/dev/i2c-*, I2C_SMBUS ioctl)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["rw_ok"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testReadWrite: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}


