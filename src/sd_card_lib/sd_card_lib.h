/**
 * @file sd_card_lib.h
 * @brief Header for SdCardTesting class (Raspberry Pi 5 SD card testing)
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#ifndef SDCARDTESTING_H
#define SDCARDTESTING_H
#include "lib_base.h"

/**
 * @class SdCardTesting
 * @brief Tests SD card presence, capacity, and block-device access.
 */
class SdCardTesting : public TestBase
{
  public:
    SdCardTesting();
    ~SdCardTesting();

    TestOutput testSdCardPresent();
    TestOutput testSdCardCapacity();
    TestOutput testSdCardBlockAccess();
    TestOutput testSdCardSpeed();

    std::vector<std::string> getAvailableTests() override;
    TestResult runSingleTest(const std::string& testName) override;

  protected:
    std::vector<TestResult> runAllTests() override;
};

#endif // SDCARDTESTING_H
