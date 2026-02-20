/**
 * @file lib_base.h
 * @brief Base class for all testing classes
 * @author sandesh ghimire
 * @copyright (C) Lucid Motors
 */
#ifndef TEST_BASE_H
#define TEST_BASE_H

#include <chrono>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <functional>

/**
 * @enum TestMode
 * @brief Enumeration for test modes
 */
enum class TestMode {
    ONE_SHOT,   ///< Run tests once
    MONITORING  ///< Run tests in a loop until stopped
};

/**
 * @struct TestOutput
 * @brief Structure to hold test output information
 */
struct TestOutput {
    bool passed;                                ///< Whether the test passed
    std::vector<std::string> data;              ///< Additional data returned by the test
};

/**
 * @struct TestResult
 * @brief Structure to hold test result information
 */
struct TestResult {
    std::string name;                           ///< Name of the test
    TestOutput output;                          ///< Test output (passed and data)
    std::chrono::duration<double> duration;     ///< Time taken for the test
};

/**
 * @class TestBase
 * @brief Base class for all testing classes
 * @details Provides common functionality for running tests in different modes
 */
class TestBase {
public:
    /**
     * @brief Constructor
     */
    TestBase();

    /**
     * @brief Destructor
     */
    virtual ~TestBase();

    /**
     * @brief Run tests in the specified mode
     * @param mode The test mode (ONE_SHOT or MONITORING)
     * @param callback Optional callback function to handle results (for monitoring mode)
     * @param interval_ms Interval between test runs in monitoring mode (default 1000ms)
     */
    void runTests(TestMode mode, std::function<void(const std::vector<TestResult>&)> callback = nullptr, int interval_ms = 1000);

    /**
     * @brief Run selected tests by name
     * @param testNames Vector of test names to run
     * @return Vector of test results
     */
    std::vector<TestResult> runSelectedTests(const std::vector<std::string>& testNames);

    /**
     * @brief Get list of all available test names
     * @return Vector of test names
     */
    virtual std::vector<std::string> getAvailableTests() = 0;

    /**
     * @brief Stop monitoring mode
     */
    void stop();

protected:
    /**
     * @brief Pure virtual method to run all tests once
     * @return Vector of test results
     */
    virtual std::vector<TestResult> runAllTests() = 0;

    /**
     * @brief Pure virtual method to run a single test by name
     * @param testName Name of the test to run
     * @return Test result with timing
     */
    virtual TestResult runSingleTest(const std::string& testName) = 0;

private:
    std::atomic<bool> stopped_;  ///< Flag to stop monitoring
    std::thread monitoring_thread_;  ///< Thread for monitoring mode
};

#endif // TEST_BASE_H