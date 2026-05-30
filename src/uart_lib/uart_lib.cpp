/**
 * @file uart_lib.cpp
 * @brief UartTesting implementation — termios (/dev/ttyS* or /dev/ttyAMA*)
 * @note HARDWARE-UNVERIFIED — on-target execution required to validate.
 */
#include "uart_lib.h"

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
UartTesting::UartTesting() = default;
UartTesting::~UartTesting() = default;

// ── TestBase interface ────────────────────────────────────────────────────────
std::vector<std::string> UartTesting::getAvailableTests() {
    return {"testPresence", "testBaudRate", "testLineParams", "testLoopbackTx"};
}

std::vector<TestResult> UartTesting::runAllTests() {
    auto names = getAvailableTests();
    return runSelectedTests(names);
}

TestResult UartTesting::runSingleTest(const std::string& testName) {
    auto names = getAvailableTests();
    for (const auto& n : names) {
        if (n == testName) return runSelectedTests({testName})[0];
    }
    TestResult r;
    r.module_id        = "uart";
    r.test_id          = testName;
    r.name             = testName;
    r.timestamp        = std::chrono::system_clock::now();
    r.output.status    = Status::ERROR;
    r.output.severity  = Severity::ERROR;
    r.output.error_message = "unknown test: " + testName;
    return r;
}

// HARDWARE-UNVERIFIED: requires real uart hardware on target.
TestOutput UartTesting::testPresence() {
    TestOutput out;
    // Detect UART device node
    // Linux interface: termios (/dev/ttyS* or /dev/ttyAMA*)
    // TODO: implement against termios (/dev/ttyS* or /dev/ttyAMA*)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["device_found"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testPresence: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real uart hardware on target.
TestOutput UartTesting::testBaudRate() {
    TestOutput out;
    // Open and read configured baud rate via tcgetattr
    // Linux interface: termios (/dev/ttyS* or /dev/ttyAMA*)
    // TODO: implement against termios (/dev/ttyS* or /dev/ttyAMA*)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["baud_rate"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testBaudRate: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real uart hardware on target.
TestOutput UartTesting::testLineParams() {
    TestOutput out;
    // Verify data bits, parity, stop bits
    // Linux interface: termios (/dev/ttyS* or /dev/ttyAMA*)
    // TODO: implement against termios (/dev/ttyS* or /dev/ttyAMA*)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["params_ok"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testLineParams: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real uart hardware on target.
TestOutput UartTesting::testLoopbackTx() {
    TestOutput out;
    // Transmit and receive a byte in loopback mode
    // Linux interface: termios (/dev/ttyS* or /dev/ttyAMA*)
    // TODO: implement against termios (/dev/ttyS* or /dev/ttyAMA*)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["loopback_ok"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testLoopbackTx: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}


