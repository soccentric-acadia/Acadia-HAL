/**
 * @file debug_and_development_interfaces.cpp
 * @brief Implementation file for DebugAndDevelopmentInterfaces class
 * @author sandesh ghimire
 * @copyright (C) Lucid Motors
 */

#include "debug_and_development_interfaces.h"

/**
 * @brief Constructor for DebugAndDevelopmentInterfaces
 */
DebugAndDevelopmentInterfaces::DebugAndDevelopmentInterfaces()
{
}

/**
 * @brief Destructor for DebugAndDevelopmentInterfaces
 */
DebugAndDevelopmentInterfaces::~DebugAndDevelopmentInterfaces()
{
}

/**
 * @brief Implementation of testJtagInterface
 * @return true (placeholder implementation)
 */
TestOutput DebugAndDevelopmentInterfaces::testJtagInterface()
{
    return {true, {}};
}

/**
 * @brief Implementation of testSerialWireDebug
 * @return true (placeholder implementation)
 */
TestOutput DebugAndDevelopmentInterfaces::testSerialWireDebug()
{
    return {true, {}};
}

/**
 * @brief Implementation of testDebugUart
 * @return true (placeholder implementation)
 */
TestOutput DebugAndDevelopmentInterfaces::testDebugUart()
{
    return {true, {}};
}

/**
 * @brief Implementation of testTracePort
 * @return true (placeholder implementation)
 */
TestOutput DebugAndDevelopmentInterfaces::testTracePort()
{
    return {true, {}};
}

/**
 * @brief Implementation of testPerformanceCounters
 * @return true (placeholder implementation)
 */
TestOutput DebugAndDevelopmentInterfaces::testPerformanceCounters()
{
    return {true, {}};
}

/**
 * @brief Implementation of testDebugAuthentication
 * @return true (placeholder implementation)
 */
TestOutput DebugAndDevelopmentInterfaces::testDebugAuthentication()
{
    return {true, {}};
}

/**
 * @brief Run all tests for this module
 * @return Vector of test results
 */
std::vector<TestResult> DebugAndDevelopmentInterfaces::runAllTests()
{
    std::vector<TestResult> results;

    // Run testJtagInterface
    {
        auto start = std::chrono::high_resolution_clock::now();
         TestOutput result = testJtagInterface();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testJtagInterface", result, duration});
    }

    // Run testSerialWireDebug
    {
        auto start = std::chrono::high_resolution_clock::now();
         TestOutput result = testSerialWireDebug();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testSerialWireDebug", result, duration});
    }

    // Run testDebugUart
    {
        auto start = std::chrono::high_resolution_clock::now();
         TestOutput result = testDebugUart();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testDebugUart", result, duration});
    }

    // Run testTracePort
    {
        auto start = std::chrono::high_resolution_clock::now();
         TestOutput result = testTracePort();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testTracePort", result, duration});
    }

    // Run testPerformanceCounters
    {
        auto start = std::chrono::high_resolution_clock::now();
         TestOutput result = testPerformanceCounters();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testPerformanceCounters", result, duration});
    }

    // Run testDebugAuthentication
    {
        auto start = std::chrono::high_resolution_clock::now();
         TestOutput result = testDebugAuthentication();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testDebugAuthentication", result, duration});
    }

    return results;
}

/**
 * @brief Get list of all available test names
 * @return Vector of test names
 */
std::vector<std::string> DebugAndDevelopmentInterfaces::getAvailableTests() {
    return {
        "testJtagInterface",
        "testSerialWireDebug",
        "testDebugUart",
        "testTracePort",
        "testPerformanceCounters",
        "testDebugAuthentication",
    };
}

/**
 * @brief Run a single test by name
 * @param testName Name of the test to run
 * @return Test result with timing
 */
TestResult DebugAndDevelopmentInterfaces::runSingleTest(const std::string& testName) {
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput result = {false, {}};

    if (testName == "testJtagInterface") {
        result = testJtagInterface();
    } else if (testName == "testSerialWireDebug") {
        result = testSerialWireDebug();
    } else if (testName == "testDebugUart") {
        result = testDebugUart();
    } else if (testName == "testTracePort") {
        result = testTracePort();
    } else if (testName == "testPerformanceCounters") {
        result = testPerformanceCounters();
    } else if (testName == "testDebugAuthentication") {
        result = testDebugAuthentication();
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
