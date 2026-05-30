/**
 * @file mipi_csi_lib.h
 * @brief MipiCsiTesting — tests the mipi_csi peripheral.
 * @details Linux interface: V4L2 (/dev/video*, VIDIOC_QUERYCAP)
 * @note HARDWARE-UNVERIFIED — all test implementations require on-target
 *       execution against real hardware.
 */
#pragma once
#include "lib_base.h"
#include <string>
#include <vector>

class MipiCsiTesting : public TestBase {
public:
    MipiCsiTesting();
    ~MipiCsiTesting() override;

    // ── TestBase interface ──────────────────────────────────────────────
    std::vector<std::string> getAvailableTests() override;
    std::vector<TestResult>  runAllTests()       override;
    TestResult               runSingleTest(const std::string& testName) override;

    // ── Module-specific tests ───────────────────────────────────────────
    TestOutput testPresence(); // HARDWARE-UNVERIFIED
    TestOutput testCapabilities(); // HARDWARE-UNVERIFIED
    TestOutput testFormat(); // HARDWARE-UNVERIFIED
    TestOutput testStreamOn(); // HARDWARE-UNVERIFIED

protected:
    std::string device_path_; // resolved device path (may be empty if not found)
};
