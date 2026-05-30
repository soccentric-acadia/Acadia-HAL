/**
 * @file can_bus_lib.h
 * @brief CanBusTesting — tests the can_bus peripheral.
 * @details Linux interface: SocketCAN (socket(AF_CAN, SOCK_RAW, CAN_RAW))
 * @note HARDWARE-UNVERIFIED — all test implementations require on-target
 *       execution against real hardware.
 */
#pragma once
#include "lib_base.h"
#include <string>
#include <vector>

class CanBusTesting : public TestBase {
public:
    CanBusTesting();
    ~CanBusTesting() override;

    // ── TestBase interface ──────────────────────────────────────────────
    std::vector<std::string> getAvailableTests() override;
    std::vector<TestResult>  runAllTests()       override;
    TestResult               runSingleTest(const std::string& testName) override;

    // ── Module-specific tests ───────────────────────────────────────────
    TestOutput testPresence(); // HARDWARE-UNVERIFIED
    TestOutput testState(); // HARDWARE-UNVERIFIED
    TestOutput testBitrate(); // HARDWARE-UNVERIFIED
    TestOutput testLoopbackTx(); // HARDWARE-UNVERIFIED

protected:
    std::string device_path_; // resolved device path (may be empty if not found)
};
