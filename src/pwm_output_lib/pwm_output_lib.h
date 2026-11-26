/**
 * @file pwm_output_lib.h
 * @brief Header for PwmOutputTesting class (Raspberry Pi 5 PWM output testing)
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#ifndef PWMOUTPUTTESTING_H
#define PWMOUTPUTTESTING_H
#include "lib_base.h"

/**
 * @class PwmOutputTesting
 * @brief Tests PWM output channels via the Linux PWM sysfs interface.
 */
class PwmOutputTesting : public TestBase
{
  public:
    PwmOutputTesting();
    ~PwmOutputTesting();

    TestOutput testPwmChipPresent();
    TestOutput testPwmExportChannel();
    TestOutput testPwmDutyCycle();

    std::vector<std::string> getAvailableTests() override;
    TestResult runSingleTest(const std::string& testName) override;

  protected:
    std::vector<TestResult> runAllTests() override;
};

#endif // PWMOUTPUTTESTING_H
