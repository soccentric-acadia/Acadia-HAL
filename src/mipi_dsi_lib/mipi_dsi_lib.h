/**
 * @file mipi_dsi_lib.h
 * @brief Header for MipiDsiTesting class (Raspberry Pi 5 MIPI DSI display testing)
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#ifndef MIPIDSITESTING_H
#define MIPIDSITESTING_H
#include "lib_base.h"

/**
 * @class MipiDsiTesting
 * @brief Tests the MIPI DSI display controller and connected DSI panel.
 */
class MipiDsiTesting : public TestBase
{
  public:
    MipiDsiTesting();
    ~MipiDsiTesting();

    TestOutput testMipiDsiControllerPresent();
    TestOutput testMipiDsiPanelConnected();
    TestOutput testMipiDsiDrmConnector();

    std::vector<std::string> getAvailableTests() override;
    TestResult runSingleTest(const std::string& testName) override;

  protected:
    std::vector<TestResult> runAllTests() override;
};

#endif // MIPIDSITESTING_H
