/**
 * @file nvme_lib.h
 * @brief NvmeTesting — tests the nvme peripheral.
 * @details Linux interface: NVMe ioctl (/dev/nvme*, NVME_IOCTL_IDENTIFY_NS)
 * @note HARDWARE-UNVERIFIED — all test implementations require on-target
 *       execution against real hardware.
 */
#pragma once
#include "lib_base.h"
#include <string>
#include <vector>

class NvmeTesting : public TestBase {
public:
    NvmeTesting();
    ~NvmeTesting() override;

    // ── TestBase interface ──────────────────────────────────────────────
    std::vector<std::string> getAvailableTests() override;
    std::vector<TestResult>  runAllTests()       override;
    TestResult               runSingleTest(const std::string& testName) override;

    // ── Module-specific tests ───────────────────────────────────────────
    TestOutput testPresence(); // HARDWARE-UNVERIFIED
    TestOutput testIdentity(); // HARDWARE-UNVERIFIED
    TestOutput testCapacity(); // HARDWARE-UNVERIFIED
    TestOutput testSmartLog(); // HARDWARE-UNVERIFIED

protected:
    std::string device_path_; // resolved device path (may be empty if not found)
};
