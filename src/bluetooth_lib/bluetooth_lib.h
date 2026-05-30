/**
 * @file bluetooth_lib.h
 * @brief BluetoothTesting — tests the bluetooth peripheral.
 * @details Linux interface: BlueZ via /sys/class/bluetooth/ and hci sockets
 * @note HARDWARE-UNVERIFIED — all test implementations require on-target
 *       execution against real hardware.
 */
#pragma once
#include "lib_base.h"
#include <string>
#include <vector>

class BluetoothTesting : public TestBase {
public:
    BluetoothTesting();
    ~BluetoothTesting() override;

    // ── TestBase interface ──────────────────────────────────────────────
    std::vector<std::string> getAvailableTests() override;
    std::vector<TestResult>  runAllTests()       override;
    TestResult               runSingleTest(const std::string& testName) override;

    // ── Module-specific tests ───────────────────────────────────────────
    TestOutput testPresence(); // HARDWARE-UNVERIFIED
    TestOutput testAddress(); // HARDWARE-UNVERIFIED
    TestOutput testPowerState(); // HARDWARE-UNVERIFIED
    TestOutput testScanResult(); // HARDWARE-UNVERIFIED

protected:
    std::string device_path_; // resolved device path (may be empty if not found)
};
