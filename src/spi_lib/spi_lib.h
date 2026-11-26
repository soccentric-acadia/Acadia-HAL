/**
 * @file spi_lib.h
 * @brief Header for SpiTesting class (Raspberry Pi 5 SPI bus testing)
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#ifndef SPITESTING_H
#define SPITESTING_H
#include "lib_base.h"

/**
 * @class SpiTesting
 * @brief Tests the Raspberry Pi 5 SPI controllers via /dev/spidev* and sysfs.
 */
class SpiTesting : public TestBase
{
  public:
    SpiTesting();
    ~SpiTesting();

    TestOutput testSpiControllerPresent();
    TestOutput testSpiDeviceEnumeration();
    TestOutput testSpiInterfaceAccess();
    TestOutput testSpiSpeedConfiguration();

    std::vector<std::string> getAvailableTests() override;
    TestResult runSingleTest(const std::string& testName) override;

  protected:
    std::vector<TestResult> runAllTests() override;
};

#endif // SPITESTING_H
