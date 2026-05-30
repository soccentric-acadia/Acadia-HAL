/**
 * @file spi_lib.h
 * @brief SpiTesting — tests the spi peripheral.
 * @details Linux interface: spidev (/dev/spidev*, SPI_IOC_MESSAGE)
 * @note HARDWARE-UNVERIFIED — all test implementations require on-target
 *       execution against real hardware.
 */
#pragma once
#include "lib_base.h"
#include <string>
#include <vector>

class SpiTesting : public TestBase {
public:
    SpiTesting();
    ~SpiTesting() override;

    // ── TestBase interface ──────────────────────────────────────────────
    std::vector<std::string> getAvailableTests() override;
    std::vector<TestResult>  runAllTests()       override;
    TestResult               runSingleTest(const std::string& testName) override;

    // ── Module-specific tests ───────────────────────────────────────────
    TestOutput testPresence(); // HARDWARE-UNVERIFIED
    TestOutput testMode(); // HARDWARE-UNVERIFIED
    TestOutput testMaxSpeed(); // HARDWARE-UNVERIFIED
    TestOutput testLoopbackTx(); // HARDWARE-UNVERIFIED

protected:
    std::string device_path_; // resolved device path (may be empty if not found)
};
