/**
 * @file lib_base.h
 * @brief Base class for all testing classes
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#ifndef TEST_BASE_H
#define TEST_BASE_H

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/**
 * @enum TestMode
 * @brief Enumeration for test modes
 */
enum class TestMode
{
    ONE_SHOT,   ///< Run all tests once (synchronous)
    MONITORING, ///< Run tests repeatedly at an interval until stop() is called
    LONG_TERM,  ///< Run tests at an interval for a fixed duration, then stop
    EVENT       ///< Run tests continuously, fire a callback when criteria are met
};

/**
 * @struct TestOutput
 * @brief Structure to hold test output information
 */
struct TestOutput
{
    bool passed;                   ///< Whether the test passed
    std::vector<std::string> data; ///< Additional data returned by the test
};

/**
 * @struct TestResult
 * @brief Structure to hold test result information
 */
struct TestResult
{
    std::string name;                       ///< Name of the test
    TestOutput output;                      ///< Test output (passed and data)
    std::chrono::duration<double> duration; ///< Time taken for the test
};

/**
 * @struct MonitorItem
 * @brief A named data source sampled every cycle during a monitoring test
 */
struct MonitorItem
{
    std::string name;                    ///< Display name for this item
    std::function<std::string()> reader; ///< Callable that returns current value as string
};

/**
 * @struct MonitoringSnapshot
 * @brief Full snapshot produced each cycle by runMonitoringTest()
 */
struct MonitoringSnapshot
{
    int cycle;                                                   ///< Cycle count (1-based)
    std::chrono::system_clock::time_point timestamp;             ///< Wall-clock time of snapshot
    std::vector<TestResult> testResults;                         ///< Results from running all tests
    std::vector<std::pair<std::string, std::string>> monitorData; ///< Sampled monitor item values
};

/**
 * @struct EventCriteria
 * @brief Criteria evaluated each cycle by runEventTest()
 */
struct EventCriteria
{
    std::string description;                                        ///< Human-readable description
    std::function<bool(const std::vector<TestResult>&)> predicate; ///< Returns true when event fires
};

/**
 * @class TestBase
 * @brief Base class for all testing classes
 * @details Provides four test modes:
 *   ONE_SHOT       — run all tests once (synchronous)
 *   MONITORING     — run indefinitely at a fixed interval (background thread)
 *   LONG_TERM      — run for a fixed total duration at a fixed interval (background thread)
 *   EVENT          — run continuously, fire a callback when criteria are met (background thread)
 *
 * For background modes, call stop() to abort early, or wait for isRunning() to return false.
 */
class TestBase
{
  public:
    TestBase();
    virtual ~TestBase();

    /**
     * @brief Run tests in ONE_SHOT or MONITORING mode (backward-compatible entry point)
     * @param mode        ONE_SHOT (synchronous) or MONITORING (starts background thread)
     * @param callback    Called with results each cycle
     * @param interval_ms Cycle interval for MONITORING mode (default 1000 ms)
     */
    void runTests(TestMode mode,
                  std::function<void(const std::vector<TestResult>&)> callback = nullptr,
                  int interval_ms = 1000);

    /**
     * @brief Start a long-term test that runs for a fixed total duration
     * @param interval_ms  Interval between cycles in milliseconds
     * @param duration_ms  Total run time in milliseconds; 0 = run until stop()
     * @param callback     Called after each cycle with results and 1-based cycle number
     * Starts a background thread. Call stop() to abort, or wait for isRunning() == false.
     */
    void runLongTerm(int interval_ms,
                     long long duration_ms,
                     std::function<void(const std::vector<TestResult>&, int cycle)> callback = nullptr);

    /**
     * @brief Start a monitoring test: run tests and sample hardware monitors each cycle
     * @param interval_ms  Interval between cycles in milliseconds
     * @param duration_ms  Total run time in milliseconds; 0 = run until stop()
     * @param monitorItems Additional data sources sampled each cycle
     * @param callback     Called after each cycle with a MonitoringSnapshot
     * Starts a background thread. Call stop() to abort, or wait for isRunning() == false.
     */
    void runMonitoringTest(int interval_ms,
                           long long duration_ms,
                           const std::vector<MonitorItem>& monitorItems,
                           std::function<void(const MonitoringSnapshot&)> callback = nullptr);

    /**
     * @brief Start an event-driven test: fire callback each time criteria are met
     * @param poll_interval_ms  How often to run tests and check the criteria
     * @param criteria          Event criteria (description + predicate)
     * @param event_callback    Called each time criteria.predicate returns true
     * @param max_duration_ms   Stop after this many ms even if criteria never met; 0 = indefinite
     * Starts a background thread. Call stop() to abort, or wait for isRunning() == false.
     */
    void runEventTest(int poll_interval_ms,
                      const EventCriteria& criteria,
                      std::function<void(const std::vector<TestResult>&)> event_callback,
                      long long max_duration_ms = 0);

    /**
     * @brief Run a specific subset of tests by name
     */
    std::vector<TestResult> runSelectedTests(const std::vector<std::string>& testNames);

    /**
     * @brief Return all available test names
     */
    virtual std::vector<std::string> getAvailableTests() = 0;

    /**
     * @brief Stop any active background test thread and join it
     */
    void stop();

    /**
     * @brief Returns true while a background test thread is running
     */
    bool isRunning() const;

  protected:
    virtual std::vector<TestResult> runAllTests() = 0;
    virtual TestResult runSingleTest(const std::string& testName) = 0;

  private:
    std::atomic<bool> stopped_;
    std::thread monitoring_thread_;
};

#endif // TEST_BASE_H