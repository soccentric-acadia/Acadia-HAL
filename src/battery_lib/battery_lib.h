/**
 * @file battery_lib.h
 * @brief BatteryTesting — tests the battery peripheral.
 * @details Linux interface: power-supply sysfs (/sys/class/power_supply/battery/)
 * @note HARDWARE-UNVERIFIED — all test implementations require on-target
 *       execution against real hardware.
 */
#pragma once
#include "lib_base.h"
#include <string>
#include <vector>

class BatteryTesting : public TestBase {
public:
    BatteryTesting();
    ~BatteryTesting() override;

    // ── TestBase interface ──────────────────────────────────────────────
    std::vector<std::string> getAvailableTests() override;
    std::vector<TestResult>  runAllTests()       override;
    TestResult               runSingleTest(const std::string& testName) override;

    // ── Module-specific tests ───────────────────────────────────────────
    TestOutput testPresence(); // HARDWARE-UNVERIFIED
    TestOutput testChargeLevel(); // HARDWARE-UNVERIFIED
    TestOutput testVoltage(); // HARDWARE-UNVERIFIED
    TestOutput testStatus(); // HARDWARE-UNVERIFIED

protected:
    std::string device_path_; // resolved device path (may be empty if not found)
};
