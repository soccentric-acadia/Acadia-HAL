/**
 * @file uart_lib.h
 * @brief Header for UartTesting class (Raspberry Pi 5 UART/serial testing)
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#ifndef UARTTESTING_H
#define UARTTESTING_H
#include "lib_base.h"

/**
 * @class UartTesting
 * @brief Tests the Raspberry Pi 5 PL011 UART serial ports.
 */
class UartTesting : public TestBase
{
  public:
    UartTesting();
    ~UartTesting();

    TestOutput testUartPortPresent();
    TestOutput testUartLoopback();
    TestOutput testUartBaudRate();
    TestOutput testUartBufferRead();

    std::vector<std::string> getAvailableTests() override;
    TestResult runSingleTest(const std::string& testName) override;

  protected:
    std::vector<TestResult> runAllTests() override;
};

#endif // UARTTESTING_H
