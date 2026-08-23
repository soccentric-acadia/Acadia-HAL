/**
 * @file hdmi_lib.h
 * @brief Header for HdmiTesting class (Raspberry Pi 5 / CM5 HDMI output testing)
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#ifndef HDMITESTING_H
#define HDMITESTING_H
#include "lib_base.h"

/**
 * @class HdmiTesting
 * @brief Tests HDMI video output (DRM connector) and HDMI audio (ALSA) on Pi5/CM5.
 */
class HdmiTesting : public TestBase
{
  public:
    HdmiTesting();
    ~HdmiTesting();

    TestOutput testHdmiConnectorPresent();
    TestOutput testHdmiConnected();
    TestOutput testHdmiResolution();
    TestOutput testHdmiAudio();

    std::vector<std::string> getAvailableTests() override;
    TestResult runSingleTest(const std::string& testName) override;

  protected:
    std::vector<TestResult> runAllTests() override;
};

#endif // HDMITESTING_H
