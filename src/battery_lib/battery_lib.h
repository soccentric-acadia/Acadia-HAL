/**
 * @file battery_lib.h
 * @brief Header for BatteryTesting class (Raspberry Pi 5 battery testing)
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#ifndef BATTERYTESTING_H
#define BATTERYTESTING_H
#include "lib_base.h"

/**
 * @class BatteryTesting
 * @brief Tests battery presence, charge, health, and electrical readings via
 *        /sys/class/power_supply (MXL7704 PMIC / HAT battery).
 */
class BatteryTesting : public TestBase
{
  public:
    BatteryTesting();
    ~BatteryTesting();

    TestOutput testBatteryPresent();
    TestOutput testBatteryChargeLevel();
    TestOutput testBatteryHealth();
    TestOutput testBatteryVoltageCurrent();

    std::vector<std::string> getAvailableTests() override;
    TestResult runSingleTest(const std::string& testName) override;

  protected:
    std::vector<TestResult> runAllTests() override;
};

#endif // BATTERYTESTING_H
