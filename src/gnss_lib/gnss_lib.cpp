/**
 * @file gnss_lib.cpp
 * @brief Implementation file for GnssTesting class
 * @author sandesh ghimire
 * @copyright (C) Lucid Motors
 */

#include "gnss_lib.h"

/**
 * @brief Constructor for GnssTesting
 */
GnssTesting::GnssTesting()
{
}

/**
 * @brief Destructor for GnssTesting
 */
GnssTesting::~GnssTesting()
{
}

/**
 * @brief Implementation of testGpsSignalReception
 * @return true (placeholder implementation)
 */
TestOutput GnssTesting::testGpsSignalReception()
{
    return {true, {}};
}

/**
 * @brief Implementation of testGlonassSupport
 * @return true (placeholder implementation)
 */
TestOutput GnssTesting::testGlonassSupport()
{
    return {true, {}};
}

/**
 * @brief Implementation of testGalileoSupport
 * @return true (placeholder implementation)
 */
TestOutput GnssTesting::testGalileoSupport()
{
    return {true, {}};
}

/**
 * @brief Implementation of testBeidouSupport
 * @return true (placeholder implementation)
 */
TestOutput GnssTesting::testBeidouSupport()
{
    return {true, {}};
}

/**
 * @brief Implementation of testPositionAccuracy
 * @return true (placeholder implementation)
 */
TestOutput GnssTesting::testPositionAccuracy()
{
    return {true, {}};
}

/**
 * @brief Implementation of testTimetofirstfix
 * @return true (placeholder implementation)
 */
TestOutput GnssTesting::testTimetofirstfix()
{
    return {true, {}};
}

/**
 * @brief Implementation of testGnssAntennaConnection
 * @return true (placeholder implementation)
 */
TestOutput GnssTesting::testGnssAntennaConnection()
{
    return {true, {}};
}

/**
 * @brief Implementation of testAgpsFunctionality
 * @return true (placeholder implementation)
 */
TestOutput GnssTesting::testAgpsFunctionality()
{
    return {true, {}};
}

/**
 * @brief Run all tests for this module
 * @return Vector of test results
 */
std::vector<TestResult> GnssTesting::runAllTests()
{
    std::vector<TestResult> results;

    // Run testGpsSignalReception
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testGpsSignalReception();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testGpsSignalReception", result, duration});
    }

    // Run testGlonassSupport
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testGlonassSupport();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testGlonassSupport", result, duration});
    }

    // Run testGalileoSupport
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testGalileoSupport();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testGalileoSupport", result, duration});
    }

    // Run testBeidouSupport
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testBeidouSupport();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testBeidouSupport", result, duration});
    }

    // Run testPositionAccuracy
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testPositionAccuracy();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testPositionAccuracy", result, duration});
    }

    // Run testTimetofirstfix
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testTimetofirstfix();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testTimetofirstfix", result, duration});
    }

    // Run testGnssAntennaConnection
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testGnssAntennaConnection();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testGnssAntennaConnection", result, duration});
    }

    // Run testAgpsFunctionality
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testAgpsFunctionality();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testAgpsFunctionality", result, duration});
    }

    return results;
}

/**
 * @brief Get list of all available test names
 * @return Vector of test names
 */
std::vector<std::string> GnssTesting::getAvailableTests()
{
    return {
        "testGpsSignalReception", "testGlonassSupport", "testGalileoSupport",        "testBeidouSupport",
        "testPositionAccuracy",   "testTimetofirstfix", "testGnssAntennaConnection", "testAgpsFunctionality",
    };
}

/**
 * @brief Run a single test by name
 * @param testName Name of the test to run
 * @return Test result with timing
 */
TestResult GnssTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput result = {false, {}};

    if (testName == "testGpsSignalReception") {
        result = testGpsSignalReception();
    } else if (testName == "testGlonassSupport") {
        result = testGlonassSupport();
    } else if (testName == "testGalileoSupport") {
        result = testGalileoSupport();
    } else if (testName == "testBeidouSupport") {
        result = testBeidouSupport();
    } else if (testName == "testPositionAccuracy") {
        result = testPositionAccuracy();
    } else if (testName == "testTimetofirstfix") {
        result = testTimetofirstfix();
    } else if (testName == "testGnssAntennaConnection") {
        result = testGnssAntennaConnection();
    } else if (testName == "testAgpsFunctionality") {
        result = testAgpsFunctionality();
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
