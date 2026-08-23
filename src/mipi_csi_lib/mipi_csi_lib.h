/**
 * @file mipi_csi_lib.h
 * @brief Header for MipiCsiTesting class (Raspberry Pi 5 MIPI CSI camera testing)
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#ifndef MIPICSITESTING_H
#define MIPICSITESTING_H
#include "lib_base.h"

/**
 * @class MipiCsiTesting
 * @brief Tests MIPI CSI-2 camera interface (RP1 Unicam) presence and V4L2 nodes.
 */
class MipiCsiTesting : public TestBase
{
  public:
    MipiCsiTesting();
    ~MipiCsiTesting();

    TestOutput testMipiCsiPresent();
    TestOutput testMipiCsiDeviceEnumeration();
    TestOutput testMipiCsiFormatNegotiation();

    std::vector<std::string> getAvailableTests() override;
    TestResult runSingleTest(const std::string& testName) override;

  protected:
    std::vector<TestResult> runAllTests() override;
};

#endif // MIPICSITESTING_H
