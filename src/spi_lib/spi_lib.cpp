/**
 * @file spi_lib.cpp
 * @brief SpiTesting implementation — spidev (/dev/spidev*, SPI_IOC_MESSAGE)
 * @note HARDWARE-UNVERIFIED — on-target execution required to validate.
 */
#include "spi_lib.h"

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
SpiTesting::SpiTesting() = default;
SpiTesting::~SpiTesting() = default;

// ── TestBase interface ────────────────────────────────────────────────────────
std::vector<std::string> SpiTesting::getAvailableTests() {
    return {"testPresence", "testMode", "testMaxSpeed", "testLoopbackTx"};
}

std::vector<TestResult> SpiTesting::runAllTests() {
    auto names = getAvailableTests();
    return runSelectedTests(names);
}

TestResult SpiTesting::runSingleTest(const std::string& testName) {
    auto names = getAvailableTests();
    for (const auto& n : names) {
        if (n == testName) return runSelectedTests({testName})[0];
    }
    TestResult r;
    r.module_id        = "spi";
    r.test_id          = testName;
    r.name             = testName;
    r.timestamp        = std::chrono::system_clock::now();
    r.output.status    = Status::ERROR;
    r.output.severity  = Severity::ERROR;
    r.output.error_message = "unknown test: " + testName;
    return r;
}

// HARDWARE-UNVERIFIED: requires real spi hardware on target.
TestOutput SpiTesting::testPresence() {
    TestOutput out;
    // Detect spidev device node
    // Linux interface: spidev (/dev/spidev*, SPI_IOC_MESSAGE)
    // TODO: implement against spidev (/dev/spidev*, SPI_IOC_MESSAGE)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["device_found"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testPresence: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real spi hardware on target.
TestOutput SpiTesting::testMode() {
    TestOutput out;
    // Read and verify SPI mode (CPOL/CPHA)
    // Linux interface: spidev (/dev/spidev*, SPI_IOC_MESSAGE)
    // TODO: implement against spidev (/dev/spidev*, SPI_IOC_MESSAGE)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["spi_mode"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testMode: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real spi hardware on target.
TestOutput SpiTesting::testMaxSpeed() {
    TestOutput out;
    // Read maximum clock speed
    // Linux interface: spidev (/dev/spidev*, SPI_IOC_MESSAGE)
    // TODO: implement against spidev (/dev/spidev*, SPI_IOC_MESSAGE)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["max_speed_hz"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testMaxSpeed: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real spi hardware on target.
TestOutput SpiTesting::testLoopbackTx() {
    TestOutput out;
    // Send and receive a byte pattern in loopback mode
    // Linux interface: spidev (/dev/spidev*, SPI_IOC_MESSAGE)
    // TODO: implement against spidev (/dev/spidev*, SPI_IOC_MESSAGE)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["loopback_ok"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testLoopbackTx: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}


