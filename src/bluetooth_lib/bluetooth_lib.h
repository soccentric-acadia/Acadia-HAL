/**
 * @file bluetooth_lib.h
 * @brief Header for BluetoothTesting class (Raspberry Pi 5 Bluetooth testing)
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#ifndef BLUETOOTHTESTING_H
#define BLUETOOTHTESTING_H
#include "lib_base.h"

/**
 * @class BluetoothTesting
 * @brief Tests Bluetooth controller presence, power state, and HCI availability.
 */
class BluetoothTesting : public TestBase
{
  public:
    BluetoothTesting();
    ~BluetoothTesting();

    TestOutput testBluetoothControllerPresent();
    TestOutput testBluetoothPowered();
    TestOutput testBluetoothHciAvailable();

    std::vector<std::string> getAvailableTests() override;
    TestResult runSingleTest(const std::string& testName) override;

  protected:
    std::vector<TestResult> runAllTests() override;
};

#endif // BLUETOOTHTESTING_H
