/**
 * @file rtc_lib.h
 * @brief Header for RtcTesting class (Raspberry Pi 5 RTC testing)
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#ifndef RTCTESTING_H
#define RTCTESTING_H
#include "lib_base.h"

/**
 * @class RtcTesting
 * @brief Tests real-time clock presence, readability, and accuracy.
 */
class RtcTesting : public TestBase
{
  public:
    RtcTesting();
    ~RtcTesting();

    TestOutput testRtcDevicePresent();
    TestOutput testRtcReadTime();
    TestOutput testRtcAccuracy();
    TestOutput testRtcPersistence();

    std::vector<std::string> getAvailableTests() override;
    TestResult runSingleTest(const std::string& testName) override;

  protected:
    std::vector<TestResult> runAllTests() override;
};

#endif // RTCTESTING_H
