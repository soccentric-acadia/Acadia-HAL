/**
 * @file rtc_lib.h
 * @brief RtcTesting — tests the rtc peripheral.
 * @details Linux interface: RTC ioctl (/dev/rtc0, RTC_RD_TIME / RTC_SET_TIME)
 * @note HARDWARE-UNVERIFIED — all test implementations require on-target
 *       execution against real hardware.
 */
#pragma once
#include "lib_base.h"
#include <string>
#include <vector>

class RtcTesting : public TestBase {
public:
    RtcTesting();
    ~RtcTesting() override;

    // ── TestBase interface ──────────────────────────────────────────────
    std::vector<std::string> getAvailableTests() override;
    std::vector<TestResult>  runAllTests()       override;
    TestResult               runSingleTest(const std::string& testName) override;

    // ── Module-specific tests ───────────────────────────────────────────
    TestOutput testPresence(); // HARDWARE-UNVERIFIED
    TestOutput testReadTime(); // HARDWARE-UNVERIFIED
    TestOutput testAlarmRead(); // HARDWARE-UNVERIFIED
    TestOutput testAccuracy(); // HARDWARE-UNVERIFIED

protected:
    std::string device_path_; // resolved device path (may be empty if not found)
};
