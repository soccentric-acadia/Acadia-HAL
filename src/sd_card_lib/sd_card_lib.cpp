/**
 * @file sd_card_lib.cpp
 * @brief SdCardTesting implementation — MMC sysfs (/sys/class/mmc_host/mmc1/) and block device
 * @note HARDWARE-UNVERIFIED — on-target execution required to validate.
 */
#include "sd_card_lib.h"

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
SdCardTesting::SdCardTesting() = default;
SdCardTesting::~SdCardTesting() = default;

// ── TestBase interface ────────────────────────────────────────────────────────
std::vector<std::string> SdCardTesting::getAvailableTests() {
    return {"testPresence", "testCapacity", "testCID", "testReadWrite"};
}

std::vector<TestResult> SdCardTesting::runAllTests() {
    auto names = getAvailableTests();
    return runSelectedTests(names);
}

TestResult SdCardTesting::runSingleTest(const std::string& testName) {
    auto names = getAvailableTests();
    for (const auto& n : names) {
        if (n == testName) return runSelectedTests({testName})[0];
    }
    TestResult r;
    r.module_id        = "sd_card";
    r.test_id          = testName;
    r.name             = testName;
    r.timestamp        = std::chrono::system_clock::now();
    r.output.status    = Status::ERROR;
    r.output.severity  = Severity::ERROR;
    r.output.error_message = "unknown test: " + testName;
    return r;
}

// HARDWARE-UNVERIFIED: requires real sd_card hardware on target.
TestOutput SdCardTesting::testPresence() {
    TestOutput out;
    // Detect SD card device in sysfs
    // Linux interface: MMC sysfs (/sys/class/mmc_host/mmc1/) and block device
    // TODO: implement against MMC sysfs (/sys/class/mmc_host/mmc1/) and block device
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["device_found"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testPresence: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real sd_card hardware on target.
TestOutput SdCardTesting::testCapacity() {
    TestOutput out;
    // Read SD card capacity from sysfs
    // Linux interface: MMC sysfs (/sys/class/mmc_host/mmc1/) and block device
    // TODO: implement against MMC sysfs (/sys/class/mmc_host/mmc1/) and block device
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["size_bytes"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testCapacity: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real sd_card hardware on target.
TestOutput SdCardTesting::testCID() {
    TestOutput out;
    // Read Card IDentification register
    // Linux interface: MMC sysfs (/sys/class/mmc_host/mmc1/) and block device
    // TODO: implement against MMC sysfs (/sys/class/mmc_host/mmc1/) and block device
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["cid_string"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testCID: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real sd_card hardware on target.
TestOutput SdCardTesting::testReadWrite() {
    TestOutput out;
    // Perform a small read/write test on a scratch area
    // Linux interface: MMC sysfs (/sys/class/mmc_host/mmc1/) and block device
    // TODO: implement against MMC sysfs (/sys/class/mmc_host/mmc1/) and block device
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["rw_ok"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testReadWrite: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}


