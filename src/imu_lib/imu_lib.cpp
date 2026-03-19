/**
 * @file imu_lib.cpp
 * @brief Implementation file for ImuTesting class
 * @author sandesh ghimire
 * @copyright (C) Lucid Motors
 */

#include "imu_lib.h"

/**
 * @brief Constructor for ImuTesting
 */
ImuTesting::ImuTesting()
{
}

/**
 * @brief Destructor for ImuTesting
 */
ImuTesting::~ImuTesting()
{
}

/**
 * @brief Implementation of test3axisAccelerometer
 * @return true (placeholder implementation)
 */
TestOutput ImuTesting::test3axisAccelerometer()
{
    return {true, {}};
}

/**
 * @brief Implementation of test3axisGyroscope
 * @return true (placeholder implementation)
 */
TestOutput ImuTesting::test3axisGyroscope()
{
    return {true, {}};
}

/**
 * @brief Implementation of test3axisMagnetometer
 * @return true (placeholder implementation)
 */
TestOutput ImuTesting::test3axisMagnetometer()
{
    return {true, {}};
}

/**
 * @brief Implementation of testImuDataRate
 * @return true (placeholder implementation)
 */
TestOutput ImuTesting::testImuDataRate()
{
    return {true, {}};
}

/**
 * @brief Implementation of testImuAccuracy
 * @return true (placeholder implementation)
 */
TestOutput ImuTesting::testImuAccuracy()
{
    return {true, {}};
}

/**
 * @brief Implementation of testImuCalibration
 * @return true (placeholder implementation)
 */
TestOutput ImuTesting::testImuCalibration()
{
    return {true, {}};
}

/**
 * @brief Implementation of testImuTemperatureCompensation
 * @return true (placeholder implementation)
 */
TestOutput ImuTesting::testImuTemperatureCompensation()
{
    return {true, {}};
}

/**
 * @brief Implementation of testImuToOrinInterface
 * @return true (placeholder implementation)
 */
TestOutput ImuTesting::testImuToOrinInterface()
{
    return {true, {}};
}

/**
 * @brief Run all tests for this module
 * @return Vector of test results
 */
std::vector<TestResult> ImuTesting::runAllTests()
{
    std::vector<TestResult> results;

    // Run test3axisAccelerometer
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = test3axisAccelerometer();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"test3axisAccelerometer", result, duration});
    }

    // Run test3axisGyroscope
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = test3axisGyroscope();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"test3axisGyroscope", result, duration});
    }

    // Run test3axisMagnetometer
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = test3axisMagnetometer();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"test3axisMagnetometer", result, duration});
    }

    // Run testImuDataRate
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testImuDataRate();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testImuDataRate", result, duration});
    }

    // Run testImuAccuracy
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testImuAccuracy();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testImuAccuracy", result, duration});
    }

    // Run testImuCalibration
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testImuCalibration();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testImuCalibration", result, duration});
    }

    // Run testImuTemperatureCompensation
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testImuTemperatureCompensation();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testImuTemperatureCompensation", result, duration});
    }

    // Run testImuToOrinInterface
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testImuToOrinInterface();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testImuToOrinInterface", result, duration});
    }

    return results;
}

/**
 * @brief Get list of all available test names
 * @return Vector of test names
 */
std::vector<std::string> ImuTesting::getAvailableTests()
{
    return {
        "testImuDataRate", "testImuAccuracy", "testImuCalibration", "testImuTemperatureCompensation", "testImuToOrinInterface",
    };
}

/**
 * @brief Run a single test by name
 * @param testName Name of the test to run
 * @return Test result with timing
 */
TestResult ImuTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput result = {false, {}};

    if (testName == "testImuDataRate") {
        result = testImuDataRate();
    } else if (testName == "testImuAccuracy") {
        result = testImuAccuracy();
    } else if (testName == "testImuCalibration") {
        result = testImuCalibration();
    } else if (testName == "testImuTemperatureCompensation") {
        result = testImuTemperatureCompensation();
    } else if (testName == "testImuToOrinInterface") {
        result = testImuToOrinInterface();
    } else {
        // Unknown test name
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        return {testName + " (UNKNOWN)", {false, {}}, duration};
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    return {testName, result, duration};
}
