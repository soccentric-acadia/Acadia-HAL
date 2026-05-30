/**
 * @file i2c_lib.h
 * @brief I2cTesting — tests the i2c peripheral.
 * @details Linux interface: i2c-dev (/dev/i2c-*, I2C_SMBUS ioctl)
 * @note HARDWARE-UNVERIFIED — all test implementations require on-target
 *       execution against real hardware.
 */
#pragma once
#include "lib_base.h"
#include <string>
#include <vector>

class I2cTesting : public TestBase {
public:
    I2cTesting();
    ~I2cTesting() override;

    // ── TestBase interface ──────────────────────────────────────────────
    std::vector<std::string> getAvailableTests() override;
    std::vector<TestResult>  runAllTests()       override;
    TestResult               runSingleTest(const std::string& testName) override;

    // ── Module-specific tests ───────────────────────────────────────────
    TestOutput testPresence(); // HARDWARE-UNVERIFIED
    TestOutput testBusScan(); // HARDWARE-UNVERIFIED
    TestOutput testClockStretch(); // HARDWARE-UNVERIFIED
    TestOutput testReadWrite(); // HARDWARE-UNVERIFIED

protected:
    std::string device_path_; // resolved device path (may be empty if not found)
};
