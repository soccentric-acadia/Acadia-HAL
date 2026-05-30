/**
 * @file nvme_lib.cpp
 * @brief NvmeTesting implementation — NVMe ioctl (/dev/nvme*, NVME_IOCTL_IDENTIFY_NS)
 * @note HARDWARE-UNVERIFIED — on-target execution required to validate.
 */
#include "nvme_lib.h"

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
NvmeTesting::NvmeTesting() = default;
NvmeTesting::~NvmeTesting() = default;

// ── TestBase interface ────────────────────────────────────────────────────────
std::vector<std::string> NvmeTesting::getAvailableTests() {
    return {"testPresence", "testIdentity", "testCapacity", "testSmartLog"};
}

std::vector<TestResult> NvmeTesting::runAllTests() {
    auto names = getAvailableTests();
    return runSelectedTests(names);
}

TestResult NvmeTesting::runSingleTest(const std::string& testName) {
    auto names = getAvailableTests();
    for (const auto& n : names) {
        if (n == testName) return runSelectedTests({testName})[0];
    }
    TestResult r;
    r.module_id        = "nvme";
    r.test_id          = testName;
    r.name             = testName;
    r.timestamp        = std::chrono::system_clock::now();
    r.output.status    = Status::ERROR;
    r.output.severity  = Severity::ERROR;
    r.output.error_message = "unknown test: " + testName;
    return r;
}

// HARDWARE-UNVERIFIED: requires real nvme hardware on target.
TestOutput NvmeTesting::testPresence() {
    TestOutput out;
    // Detect NVMe device node
    // Linux interface: NVMe ioctl (/dev/nvme*, NVME_IOCTL_IDENTIFY_NS)
    // TODO: implement against NVMe ioctl (/dev/nvme*, NVME_IOCTL_IDENTIFY_NS)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["device_found"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testPresence: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real nvme hardware on target.
TestOutput NvmeTesting::testIdentity() {
    TestOutput out;
    // Read NVMe identify controller
    // Linux interface: NVMe ioctl (/dev/nvme*, NVME_IOCTL_IDENTIFY_NS)
    // TODO: implement against NVMe ioctl (/dev/nvme*, NVME_IOCTL_IDENTIFY_NS)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["model_string"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testIdentity: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real nvme hardware on target.
TestOutput NvmeTesting::testCapacity() {
    TestOutput out;
    // Read namespace size
    // Linux interface: NVMe ioctl (/dev/nvme*, NVME_IOCTL_IDENTIFY_NS)
    // TODO: implement against NVMe ioctl (/dev/nvme*, NVME_IOCTL_IDENTIFY_NS)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["size_bytes"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testCapacity: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real nvme hardware on target.
TestOutput NvmeTesting::testSmartLog() {
    TestOutput out;
    // Read SMART / health information log
    // Linux interface: NVMe ioctl (/dev/nvme*, NVME_IOCTL_IDENTIFY_NS)
    // TODO: implement against NVMe ioctl (/dev/nvme*, NVME_IOCTL_IDENTIFY_NS)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["temperature_c"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testSmartLog: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}


