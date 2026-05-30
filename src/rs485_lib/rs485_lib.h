/**
 * @file rs485_lib.h
 * @brief Rs485Testing — tests the rs485 peripheral.
 * @details Linux interface: termios + RS-485 ioctl TIOCSRS485 (/dev/ttyS*)
 * @note HARDWARE-UNVERIFIED — all test implementations require on-target
 *       execution against real hardware.
 */
#pragma once
#include "lib_base.h"
#include <string>
#include <vector>

class Rs485Testing : public TestBase {
public:
    Rs485Testing();
    ~Rs485Testing() override;

    // ── TestBase interface ──────────────────────────────────────────────
    std::vector<std::string> getAvailableTests() override;
    std::vector<TestResult>  runAllTests()       override;
    TestResult               runSingleTest(const std::string& testName) override;

    // ── Module-specific tests ───────────────────────────────────────────
    TestOutput testPresence(); // HARDWARE-UNVERIFIED
    TestOutput testRs485Enable(); // HARDWARE-UNVERIFIED
    TestOutput testBaudRate(); // HARDWARE-UNVERIFIED
    TestOutput testLoopbackTx(); // HARDWARE-UNVERIFIED

protected:
    std::string device_path_; // resolved device path (may be empty if not found)
};
