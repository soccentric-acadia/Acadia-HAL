/**
 * @file accelerometer_lib.h
 * @brief Header for AccelerometerTesting class (Raspberry Pi 5 accelerometer testing)
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#ifndef ACCELEROMETERTESTING_H
#define ACCELEROMETERTESTING_H
#include "lib_base.h"

/**
 * @class AccelerometerTesting
 * @brief Tests an accelerometer (e.g. LSM6DSM, BNO055) exposed via the IIO
 *        subsystem on the Raspberry Pi 5.
 */
class AccelerometerTesting : public TestBase
{
  public:
    AccelerometerTesting();
    ~AccelerometerTesting();

    TestOutput testAccelerometerPresent();
    TestOutput testAccelerometerRead();
    TestOutput testAccelerometerScale();

    std::vector<std::string> getAvailableTests() override;
    TestResult runSingleTest(const std::string& testName) override;

  protected:
    std::vector<TestResult> runAllTests() override;
};

#endif // ACCELEROMETERTESTING_H
