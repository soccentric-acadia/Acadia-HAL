/**
 * @file emmc_lib.h
 * @brief EmmcTesting — tests the emmc peripheral.
 * @details Linux interface: MMC sysfs (/sys/class/mmc_host/mmc0/) and block device
 * @note HARDWARE-UNVERIFIED — all test implementations require on-target
 *       execution against real hardware.
 */
#pragma once
#include "lib_base.h"
#include <string>
#include <vector>

class EmmcTesting : public TestBase {
public:
    EmmcTesting();
    ~EmmcTesting() override;

    // ── TestBase interface ──────────────────────────────────────────────
    std::vector<std::string> getAvailableTests() override;
    std::vector<TestResult>  runAllTests()       override;
    TestResult               runSingleTest(const std::string& testName) override;

    // ── Module-specific tests ───────────────────────────────────────────
    TestOutput testPresence(); // HARDWARE-UNVERIFIED
    TestOutput testCapacity(); // HARDWARE-UNVERIFIED
    TestOutput testCID(); // HARDWARE-UNVERIFIED
    TestOutput testReadWrite(); // HARDWARE-UNVERIFIED

protected:
    std::string device_path_; // resolved device path (may be empty if not found)
};
