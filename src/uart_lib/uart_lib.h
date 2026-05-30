/**
 * @file uart_lib.h
 * @brief UartTesting — tests the uart peripheral.
 * @details Linux interface: termios (/dev/ttyS* or /dev/ttyAMA*)
 * @note HARDWARE-UNVERIFIED — all test implementations require on-target
 *       execution against real hardware.
 */
#pragma once
#include "lib_base.h"
#include <string>
#include <vector>

class UartTesting : public TestBase {
public:
    UartTesting();
    ~UartTesting() override;

    // ── TestBase interface ──────────────────────────────────────────────
    std::vector<std::string> getAvailableTests() override;
    std::vector<TestResult>  runAllTests()       override;
    TestResult               runSingleTest(const std::string& testName) override;

    // ── Module-specific tests ───────────────────────────────────────────
    TestOutput testPresence(); // HARDWARE-UNVERIFIED
    TestOutput testBaudRate(); // HARDWARE-UNVERIFIED
    TestOutput testLineParams(); // HARDWARE-UNVERIFIED
    TestOutput testLoopbackTx(); // HARDWARE-UNVERIFIED

protected:
    std::string device_path_; // resolved device path (may be empty if not found)
};
