/**
 * @file can_bus_lib.cpp
 * @brief CanBusTesting implementation — SocketCAN (socket(AF_CAN, SOCK_RAW, CAN_RAW))
 * @note HARDWARE-UNVERIFIED — on-target execution required to validate.
 */
#include "can_bus_lib.h"

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
CanBusTesting::CanBusTesting() = default;
CanBusTesting::~CanBusTesting() = default;

// ── TestBase interface ────────────────────────────────────────────────────────
std::vector<std::string> CanBusTesting::getAvailableTests() {
    return {"testPresence", "testState", "testBitrate", "testLoopbackTx"};
}

std::vector<TestResult> CanBusTesting::runAllTests() {
    auto names = getAvailableTests();
    return runSelectedTests(names);
}

TestResult CanBusTesting::runSingleTest(const std::string& testName) {
    auto names = getAvailableTests();
    for (const auto& n : names) {
        if (n == testName) return runSelectedTests({testName})[0];
    }
    TestResult r;
    r.module_id        = "can_bus";
    r.test_id          = testName;
    r.name             = testName;
    r.timestamp        = std::chrono::system_clock::now();
    r.output.status    = Status::ERROR;
    r.output.severity  = Severity::ERROR;
    r.output.error_message = "unknown test: " + testName;
    return r;
}

// HARDWARE-UNVERIFIED: requires real can_bus hardware on target.
TestOutput CanBusTesting::testPresence() {
    TestOutput out;
    // Detect CAN network interface
    // Linux interface: SocketCAN (socket(AF_CAN, SOCK_RAW, CAN_RAW))
    // TODO: implement against SocketCAN (socket(AF_CAN, SOCK_RAW, CAN_RAW))
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["iface_found"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testPresence: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real can_bus hardware on target.
TestOutput CanBusTesting::testState() {
    TestOutput out;
    // Read CAN controller bus state
    // Linux interface: SocketCAN (socket(AF_CAN, SOCK_RAW, CAN_RAW))
    // TODO: implement against SocketCAN (socket(AF_CAN, SOCK_RAW, CAN_RAW))
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["bus_state"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testState: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real can_bus hardware on target.
TestOutput CanBusTesting::testBitrate() {
    TestOutput out;
    // Read configured bitrate from sysfs
    // Linux interface: SocketCAN (socket(AF_CAN, SOCK_RAW, CAN_RAW))
    // TODO: implement against SocketCAN (socket(AF_CAN, SOCK_RAW, CAN_RAW))
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["bitrate_bps"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testBitrate: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real can_bus hardware on target.
TestOutput CanBusTesting::testLoopbackTx() {
    TestOutput out;
    // Send and receive a CAN frame in loopback mode
    // Linux interface: SocketCAN (socket(AF_CAN, SOCK_RAW, CAN_RAW))
    // TODO: implement against SocketCAN (socket(AF_CAN, SOCK_RAW, CAN_RAW))
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["loopback_ok"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testLoopbackTx: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}


