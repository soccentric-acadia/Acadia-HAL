/**
 * @file pwm_output_lib.h
 * @brief PwmOutputTesting — tests the pwm_output peripheral.
 * @details Linux interface: PWM sysfs (/sys/class/pwm/pwmchip0/)
 * @note HARDWARE-UNVERIFIED — all test implementations require on-target
 *       execution against real hardware.
 */
#pragma once
#include "lib_base.h"
#include <string>
#include <vector>

class PwmOutputTesting : public TestBase {
public:
    PwmOutputTesting();
    ~PwmOutputTesting() override;

    // ── TestBase interface ──────────────────────────────────────────────
    std::vector<std::string> getAvailableTests() override;
    std::vector<TestResult>  runAllTests()       override;
    TestResult               runSingleTest(const std::string& testName) override;

    // ── Module-specific tests ───────────────────────────────────────────
    TestOutput testPresence(); // HARDWARE-UNVERIFIED
    TestOutput testExport(); // HARDWARE-UNVERIFIED
    TestOutput testPeriod(); // HARDWARE-UNVERIFIED
    TestOutput testDutyCycle(); // HARDWARE-UNVERIFIED

protected:
    std::string device_path_; // resolved device path (may be empty if not found)
};
