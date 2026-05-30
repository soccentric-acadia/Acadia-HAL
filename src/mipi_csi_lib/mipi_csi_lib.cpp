/**
 * @file mipi_csi_lib.cpp
 * @brief MipiCsiTesting implementation — V4L2 (/dev/video*, VIDIOC_QUERYCAP)
 * @note HARDWARE-UNVERIFIED — on-target execution required to validate.
 */
#include "mipi_csi_lib.h"

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
MipiCsiTesting::MipiCsiTesting() = default;
MipiCsiTesting::~MipiCsiTesting() = default;

// ── TestBase interface ────────────────────────────────────────────────────────
std::vector<std::string> MipiCsiTesting::getAvailableTests() {
    return {"testPresence", "testCapabilities", "testFormat", "testStreamOn"};
}

std::vector<TestResult> MipiCsiTesting::runAllTests() {
    auto names = getAvailableTests();
    return runSelectedTests(names);
}

TestResult MipiCsiTesting::runSingleTest(const std::string& testName) {
    auto names = getAvailableTests();
    for (const auto& n : names) {
        if (n == testName) return runSelectedTests({testName})[0];
    }
    TestResult r;
    r.module_id        = "mipi_csi";
    r.test_id          = testName;
    r.name             = testName;
    r.timestamp        = std::chrono::system_clock::now();
    r.output.status    = Status::ERROR;
    r.output.severity  = Severity::ERROR;
    r.output.error_message = "unknown test: " + testName;
    return r;
}

// HARDWARE-UNVERIFIED: requires real mipi_csi hardware on target.
TestOutput MipiCsiTesting::testPresence() {
    TestOutput out;
    // Detect V4L2 capture device
    // Linux interface: V4L2 (/dev/video*, VIDIOC_QUERYCAP)
    // TODO: implement against V4L2 (/dev/video*, VIDIOC_QUERYCAP)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["device_found"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testPresence: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real mipi_csi hardware on target.
TestOutput MipiCsiTesting::testCapabilities() {
    TestOutput out;
    // Read VIDIOC_QUERYCAP capability flags
    // Linux interface: V4L2 (/dev/video*, VIDIOC_QUERYCAP)
    // TODO: implement against V4L2 (/dev/video*, VIDIOC_QUERYCAP)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["cap_flags"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testCapabilities: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real mipi_csi hardware on target.
TestOutput MipiCsiTesting::testFormat() {
    TestOutput out;
    // Enumerate and set a capture format
    // Linux interface: V4L2 (/dev/video*, VIDIOC_QUERYCAP)
    // TODO: implement against V4L2 (/dev/video*, VIDIOC_QUERYCAP)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["format_ok"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testFormat: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}

// HARDWARE-UNVERIFIED: requires real mipi_csi hardware on target.
TestOutput MipiCsiTesting::testStreamOn() {
    TestOutput out;
    // Start and stop streaming (STREAMON/STREAMOFF)
    // Linux interface: V4L2 (/dev/video*, VIDIOC_QUERYCAP)
    // TODO: implement against V4L2 (/dev/video*, VIDIOC_QUERYCAP)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["stream_ok"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testStreamOn: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}


