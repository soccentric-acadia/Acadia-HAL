/**
 * @file i2c_lib.h
 * @brief Header for I2cTesting class (Raspberry Pi 5 I2C bus testing)
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#ifndef I2CTESTING_H
#define I2CTESTING_H
#include "lib_base.h"

/**
 * @class I2cTesting
 * @brief Tests the Raspberry Pi 5 I2C bus controllers via /dev/i2c-* and sysfs.
 * @details Verifies I2C controller presence, device enumeration, bus scanning,
 *          and interface access.
 */
class I2cTesting : public TestBase
{
  public:
    I2cTesting();
    ~I2cTesting();

    TestOutput testI2cControllerPresent();
    TestOutput testI2cBusScan();
    TestOutput testI2cDeviceEnumeration();
    TestOutput testI2cInterfaceAccess();

    std::vector<std::string> getAvailableTests() override;
    TestResult runSingleTest(const std::string& testName) override;

  protected:
    std::vector<TestResult> runAllTests() override;
};

#endif // I2CTESTING_H
