/**
 * @file display_interfaces.cpp
 * @brief Implementation file for DisplayInterfaces class
 * @author sandesh ghimire
 * @copyright (C) Lucid Motors
 */

#include "display_interfaces.h"

/**
 * @brief Constructor for DisplayInterfaces
 */
DisplayInterfaces::DisplayInterfaces()
{
}

/**
 * @brief Destructor for DisplayInterfaces
 */
DisplayInterfaces::~DisplayInterfaces()
{
}

/**
 * @brief Implementation of testDisplayPortOutput
 * @return true (placeholder implementation)
 */
TestOutput DisplayInterfaces::testDisplayPortOutput()
{
    return {true, {}};
}

/**
 * @brief Implementation of testHdmiOutput
 * @return true (placeholder implementation)
 */
TestOutput DisplayInterfaces::testHdmiOutput()
{
    return {true, {}};
}

/**
 * @brief Implementation of testMultiDisplaySupport
 * @return true (placeholder implementation)
 */
TestOutput DisplayInterfaces::testMultiDisplaySupport()
{
    return {true, {}};
}

/**
 * @brief Implementation of testVariousResolutions
 * @return true (placeholder implementation)
 */
TestOutput DisplayInterfaces::testVariousResolutions()
{
    return {true, {}};
}

/**
 * @brief Implementation of testVariousRefreshRates
 * @return true (placeholder implementation)
 */
TestOutput DisplayInterfaces::testVariousRefreshRates()
{
    return {true, {}};
}

/**
 * @brief Implementation of testHdrSupport
 * @return true (placeholder implementation)
 */
TestOutput DisplayInterfaces::testHdrSupport()
{
    return {true, {}};
}

/**
 * @brief Implementation of testDisplayHotPlugDetection
 * @return true (placeholder implementation)
 */
TestOutput DisplayInterfaces::testDisplayHotPlugDetection()
{
    return {true, {}};
}

/**
 * @brief Run all tests for this module
 * @return Vector of test results
 */
std::vector<TestResult> DisplayInterfaces::runAllTests()
{
    std::vector<TestResult> results;

    // Run testDisplayPortOutput
    {
        auto start = std::chrono::high_resolution_clock::now();
         TestOutput result = testDisplayPortOutput();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testDisplayPortOutput", result, duration});
    }

    // Run testHdmiOutput
    {
        auto start = std::chrono::high_resolution_clock::now();
         TestOutput result = testHdmiOutput();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testHdmiOutput", result, duration});
    }

    // Run testMultiDisplaySupport
    {
        auto start = std::chrono::high_resolution_clock::now();
         TestOutput result = testMultiDisplaySupport();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testMultiDisplaySupport", result, duration});
    }

    // Run testVariousResolutions
    {
        auto start = std::chrono::high_resolution_clock::now();
         TestOutput result = testVariousResolutions();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testVariousResolutions", result, duration});
    }

    // Run testVariousRefreshRates
    {
        auto start = std::chrono::high_resolution_clock::now();
         TestOutput result = testVariousRefreshRates();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testVariousRefreshRates", result, duration});
    }

    // Run testHdrSupport
    {
        auto start = std::chrono::high_resolution_clock::now();
         TestOutput result = testHdrSupport();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testHdrSupport", result, duration});
    }

    // Run testDisplayHotPlugDetection
    {
        auto start = std::chrono::high_resolution_clock::now();
         TestOutput result = testDisplayHotPlugDetection();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testDisplayHotPlugDetection", result, duration});
    }

    return results;
}

/**
 * @brief Get list of all available test names
 * @return Vector of test names
 */
std::vector<std::string> DisplayInterfaces::getAvailableTests() {
    return {
        "testDisplayPortOutput",
        "testHdmiOutput",
        "testMultiDisplaySupport",
        "testVariousResolutions",
        "testVariousRefreshRates",
        "testHdrSupport",
        "testDisplayHotPlugDetection",
    };
}

/**
 * @brief Run a single test by name
 * @param testName Name of the test to run
 * @return Test result with timing
 */
TestResult DisplayInterfaces::runSingleTest(const std::string& testName) {
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput result = {false, {}};

    if (testName == "testDisplayPortOutput") {
        result = testDisplayPortOutput();
    } else if (testName == "testHdmiOutput") {
        result = testHdmiOutput();
    } else if (testName == "testMultiDisplaySupport") {
        result = testMultiDisplaySupport();
    } else if (testName == "testVariousResolutions") {
        result = testVariousResolutions();
    } else if (testName == "testVariousRefreshRates") {
        result = testVariousRefreshRates();
    } else if (testName == "testHdrSupport") {
        result = testHdrSupport();
    } else if (testName == "testDisplayHotPlugDetection") {
        result = testDisplayHotPlugDetection();
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
