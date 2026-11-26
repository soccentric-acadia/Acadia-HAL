/**
 * @file fan_lib.h
 * @brief Header for FanTesting class (Raspberry Pi 5 / CM5 cooling fan testing)
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#ifndef FANTESTING_H
#define FANTESTING_H
#include "lib_base.h"

/**
 * @class FanTesting
 * @brief Tests the PWM cooling fan (Pi5 5-pin fan connector / CM5 carrier) via
 *        the thermal cooling_device and PWM sysfs interfaces.
 */
class FanTesting : public TestBase
{
  public:
    FanTesting();
    ~FanTesting();

    TestOutput testFanPresent();
    TestOutput testFanThermalZone();
    TestOutput testFanControl();

    std::vector<std::string> getAvailableTests() override;
    TestResult runSingleTest(const std::string& testName) override;

  protected:
    std::vector<TestResult> runAllTests() override;
};

#endif // FANTESTING_H
