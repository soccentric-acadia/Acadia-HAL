/**
 * @file analog_input_lib.h
 * @brief AnalogInputTesting — tests the analog_input peripheral.
 * @details Linux interface: IIO sysfs (/sys/bus/iio/devices/iio:device*, voltage channels)
 * @note HARDWARE-UNVERIFIED — all test implementations require on-target
 *       execution against real hardware.
 */
#pragma once
#include "lib_base.h"
#include <string>
#include <vector>

class AnalogInputTesting : public TestBase {
public:
    AnalogInputTesting();
    ~AnalogInputTesting() override;

    // ── TestBase interface ──────────────────────────────────────────────
    std::vector<std::string> getAvailableTests() override;
    std::vector<TestResult>  runAllTests()       override;
    TestResult               runSingleTest(const std::string& testName) override;

    // ── Module-specific tests ───────────────────────────────────────────
    TestOutput testPresence(); // HARDWARE-UNVERIFIED
    TestOutput testChannelRead(); // HARDWARE-UNVERIFIED
    TestOutput testResolution(); // HARDWARE-UNVERIFIED
    TestOutput testSampleRate(); // HARDWARE-UNVERIFIED

protected:
    std::string device_path_; // resolved device path (may be empty if not found)
};
