/**
 * @file rs485_lib.h
 * @brief Header for Rs485Testing class (Raspberry Pi 5 RS-485 bus testing)
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#ifndef RS485TESTING_H
#define RS485TESTING_H
#include "lib_base.h"

/**
 * @class Rs485Testing
 * @brief Tests RS-485 half-duplex serial bus capability via the PL011 UART.
 */
class Rs485Testing : public TestBase
{
  public:
    Rs485Testing();
    ~Rs485Testing();

    TestOutput testRs485PortPresent();
    TestOutput testRs485ModeConfigure();
    TestOutput testRs485HalfDuplex();

    std::vector<std::string> getAvailableTests() override;
    TestResult runSingleTest(const std::string& testName) override;

  protected:
    std::vector<TestResult> runAllTests() override;
};

#endif // RS485TESTING_H
