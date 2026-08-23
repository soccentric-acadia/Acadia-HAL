/**
 * @file wifi_lib.h
 * @brief Header for WifiTesting class (Raspberry Pi 5 / CM5 Wi-Fi testing)
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#ifndef WIFITESTING_H
#define WIFITESTING_H
#include "lib_base.h"

/**
 * @class WifiTesting
 * @brief Tests the built-in BCM2805 802.11ac Wi-Fi radio (wlan0) on Pi5 / CM5.
 */
class WifiTesting : public TestBase
{
  public:
    WifiTesting();
    ~WifiTesting();

    TestOutput testWifiInterfacePresent();
    TestOutput testWifiLinkUp();
    TestOutput testWifiSignalStrength();
    TestOutput testWifiScan();

    std::vector<std::string> getAvailableTests() override;
    TestResult runSingleTest(const std::string& testName) override;

  protected:
    std::vector<TestResult> runAllTests() override;
};

#endif // WIFITESTING_H
