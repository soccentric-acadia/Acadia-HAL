/**
 * @file accelerometer_lib.h
 * @brief AccelerometerTesting — tests the accelerometer peripheral.
 * @details Linux interface: IIO sysfs (/sys/bus/iio/devices/iio:device*)
 * @note HARDWARE-UNVERIFIED — all test implementations require on-target
 *       execution against real hardware.
 */
#pragma once
#include "lib_base.h"
#include <string>
#include <vector>

class AccelerometerTesting : public TestBase {
public:
    AccelerometerTesting();
    ~AccelerometerTesting() override;

    // ── TestBase interface ──────────────────────────────────────────────
    std::vector<std::string> getAvailableTests() override;
    std::vector<TestResult>  runAllTests()       override;
    TestResult               runSingleTest(const std::string& testName) override;

    // ── Module-specific tests ───────────────────────────────────────────
    TestOutput testPresence(); // HARDWARE-UNVERIFIED
    TestOutput testAxisReadback(); // HARDWARE-UNVERIFIED
    TestOutput testDataRate(); // HARDWARE-UNVERIFIED
    TestOutput testSelfTest(); // HARDWARE-UNVERIFIED

protected:
    std::string device_path_; // resolved device path (may be empty if not found)
};
