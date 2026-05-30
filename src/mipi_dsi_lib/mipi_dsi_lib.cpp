/**
 * @file mipi_dsi_lib.cpp
 * @brief MipiDsiTesting implementation — DRM/KMS (/sys/class/drm/ and drmIoctl)
 * @note HARDWARE-UNVERIFIED — on-target execution required to validate.
 */
#include "mipi_dsi_lib.h"

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
MipiDsiTesting::MipiDsiTesting() = default;
MipiDsiTesting::~MipiDsiTesting() = default;

// ── TestBase interface ────────────────────────────────────────────────────────
std::vector<std::string> MipiDsiTesting::getAvailableTests() {
    return {"testPresence", "testStatus", "testModes", "testFramebuffer"};
}

std::vector<TestResult> MipiDsiTesting::runAllTests() {
    auto names = getAvailableTests();
    return runSelectedTests(names);
}

TestResult MipiDsiTesting::runSingleTest(const std::string& testName) {
    auto names = getAvailableTests();
    for (const auto& n : names) {
        if (n == testName) return runSelectedTests({testName})[0];
    }
    TestResult r;
    r.module_id        = "mipi_dsi";
    r.test_id          = testName;
    r.name             = testName;
    r.timestamp        = std::chrono::system_clock::now();
    r.output.status    = Status::ERROR;
    r.output.severity  = Severity::ERROR;
    r.output.error_message = "unknown test: " + testName;
    return r;
}

// HARDWARE-UNVERIFIED: requires real mipi_dsi hardware on target.
TestOutput MipiDsiTesting::testPresence() {
    TestOutput out;
    // Detect DRM connector in sysfs
    // Linux interface: DRM/KMS (/sys/class/drm/ and drmIoctl)
    // TODO: implement against DRM/KMS (/sys/class/drm/ and drmIoctl)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["connector_found"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testPresence: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real mipi_dsi hardware on target.
TestOutput MipiDsiTesting::testStatus() {
    TestOutput out;
    // Check connector connection status
    // Linux interface: DRM/KMS (/sys/class/drm/ and drmIoctl)
    // TODO: implement against DRM/KMS (/sys/class/drm/ and drmIoctl)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["connected"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testStatus: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real mipi_dsi hardware on target.
TestOutput MipiDsiTesting::testModes() {
    TestOutput out;
    // Count available display modes
    // Linux interface: DRM/KMS (/sys/class/drm/ and drmIoctl)
    // TODO: implement against DRM/KMS (/sys/class/drm/ and drmIoctl)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["mode_count"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testModes: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real mipi_dsi hardware on target.
TestOutput MipiDsiTesting::testFramebuffer() {
    TestOutput out;
    // Allocate and release a framebuffer
    // Linux interface: DRM/KMS (/sys/class/drm/ and drmIoctl)
    // TODO: implement against DRM/KMS (/sys/class/drm/ and drmIoctl)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["fb_ok"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testFramebuffer: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}


