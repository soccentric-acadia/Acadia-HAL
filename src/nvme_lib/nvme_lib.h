/**
 * @file nvme_lib.h
 * @brief Header for NvmeTesting class (Raspberry Pi 5 NVMe SSD testing)
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#ifndef NVMEETESTING_H
#define NVMEETESTING_H
#include "lib_base.h"

/**
 * @class NvmeTesting
 * @brief Tests NVMe SSD presence, health (SMART), and block-device access.
 */
class NvmeTesting : public TestBase
{
  public:
    NvmeTesting();
    ~NvmeTesting();

    TestOutput testNvmeDevicePresent();
    TestOutput testNvmeHealth();
    TestOutput testNvmeCapacity();
    TestOutput testNvmeBlockAccess();

    std::vector<std::string> getAvailableTests() override;
    TestResult runSingleTest(const std::string& testName) override;

  protected:
    std::vector<TestResult> runAllTests() override;
};

#endif // NVMEETESTING_H
