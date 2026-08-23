/**
 * @file emmc_lib.h
 * @brief Header for EmmcTesting class (Raspberry Pi 5 eMMC storage testing)
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#ifndef EMMCTESTING_H
#define EMMCTESTING_H
#include "lib_base.h"

/**
 * @class EmmcTesting
 * @brief Tests eMMC / MMC flash storage presence, capacity, and access.
 */
class EmmcTesting : public TestBase
{
  public:
    EmmcTesting();
    ~EmmcTesting();

    TestOutput testEmmcDevicePresent();
    TestOutput testEmmcCapacity();
    TestOutput testEmmcBlockAccess();
    TestOutput testEmmcType();

    std::vector<std::string> getAvailableTests() override;
    TestResult runSingleTest(const std::string& testName) override;

  protected:
    std::vector<TestResult> runAllTests() override;
};

#endif // EMMCTESTING_H
