/**
 * @file analog_input_lib.h
 * @brief Header for AnalogInputTesting class (Raspberry Pi 5 ADC testing)
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#ifndef ANALOGINPUTTESTING_H
#define ANALOGINPUTTESTING_H
#include "lib_base.h"

/**
 * @class AnalogInputTesting
 * @brief Tests analog input (ADC) device presence and channel readability via
 *        the Linux IIO subsystem.
 */
class AnalogInputTesting : public TestBase
{
  public:
    AnalogInputTesting();
    ~AnalogInputTesting();

    TestOutput testAdcDevicePresent();
    TestOutput testAdcChannelRead();
    TestOutput testAdcResolution();

    std::vector<std::string> getAvailableTests() override;
    TestResult runSingleTest(const std::string& testName) override;

  protected:
    std::vector<TestResult> runAllTests() override;
};

#endif // ANALOGINPUTTESTING_H
