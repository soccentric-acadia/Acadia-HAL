/**
 * @file can_bus_lib.h
 * @brief Header for CanBusTesting class (Raspberry Pi 5 CAN bus testing)
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#ifndef CANBUSTESTING_H
#define CANBUSTESTING_H
#include "lib_base.h"

/**
 * @class CanBusTesting
 * @brief Tests CAN bus interface presence, link state, and transmission.
 *        (Pi5 has no native CAN; an SPI MCP2515 HAT exposes can0.)
 */
class CanBusTesting : public TestBase
{
  public:
    CanBusTesting();
    ~CanBusTesting();

    TestOutput testCanControllerPresent();
    TestOutput testCanInterfaceState();
    TestOutput testCanLinkActive();

    std::vector<std::string> getAvailableTests() override;
    TestResult runSingleTest(const std::string& testName) override;

  protected:
    std::vector<TestResult> runAllTests() override;
};

#endif // CANBUSTESTING_H
