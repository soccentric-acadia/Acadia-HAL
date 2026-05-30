/**
 * @file mipi_dsi_lib.h
 * @brief MipiDsiTesting — tests the mipi_dsi peripheral.
 * @details Linux interface: DRM/KMS (/sys/class/drm/ and drmIoctl)
 * @note HARDWARE-UNVERIFIED — all test implementations require on-target
 *       execution against real hardware.
 */
#pragma once
#include "lib_base.h"
#include <string>
#include <vector>

class MipiDsiTesting : public TestBase {
public:
    MipiDsiTesting();
    ~MipiDsiTesting() override;

    // ── TestBase interface ──────────────────────────────────────────────
    std::vector<std::string> getAvailableTests() override;
    std::vector<TestResult>  runAllTests()       override;
    TestResult               runSingleTest(const std::string& testName) override;

    // ── Module-specific tests ───────────────────────────────────────────
    TestOutput testPresence(); // HARDWARE-UNVERIFIED
    TestOutput testStatus(); // HARDWARE-UNVERIFIED
    TestOutput testModes(); // HARDWARE-UNVERIFIED
    TestOutput testFramebuffer(); // HARDWARE-UNVERIFIED

protected:
    std::string device_path_; // resolved device path (may be empty if not found)
};
