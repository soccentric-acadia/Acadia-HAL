/**
 * @file lib_base.h
 * @brief Base class for all testing classes
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

/**
 * @enum TestMode
 * @brief Enumeration for test modes
 */
// HAL-002: values match protos/common.proto ExecutionMode (1..4) so the HAL
// enum can be mapped to the wire enum 1:1 without magic-number translation.
enum class TestMode
{
    SINGLE_SHOT = 1,  ///< Run all tests once (synchronous)
    LONG_TERM   = 2,  ///< Run tests at an interval for a fixed duration, then stop
    MONITORING  = 3,  ///< Run tests repeatedly at an interval until stop() is called
    EVENT_BASED = 4   ///< Run tests continuously, fire a callback when criteria are met
};

// ============================================================
// Canonical result types (hal namespace)
// ============================================================

namespace hal {

/// Typed value used for metrics, monitoring readings, and event payloads.
/// Strings remain available for non-numeric data; numeric/bool data must NOT
/// be stringified — downstream consumers (charting, thresholds) rely on type.
using Value = std::variant<bool, int64_t, double, std::string>;

/// Terminal outcome of a single test execution.
enum class Status {
    PASS,
    FAIL,
    ERROR,
    TIMEOUT,
    SKIPPED,
    ABORTED,
};

/// Severity classifies how serious a non-PASS outcome is, or how informational
/// a PASS is.
enum class Severity {
    INFO,
    WARNING,
    ERROR,
    CRITICAL,
};

/// Output of a single test execution.
struct TestOutput {
    Status status{Status::ERROR};
    Severity severity{Severity::INFO};
    std::map<std::string, std::string> reason;
    std::map<std::string, Value> metrics;
    std::map<std::string, Value> monitoring;
    std::map<std::string, Value> event;
    std::string error_message;
};

/// Task 63: stamp a vector of diagnostic strings into STABLE TYPED metrics
/// instead of positional `reason["detail_<n>"]` entries. Each line is parsed
/// as a number when possible (so consumers read a typed Value, not a position);
/// otherwise it is stored as a string Value. The `metrics` map is the stable
/// contract consumers rely on.
static inline void StampDiagnostics(TestOutput& out,
                                    const std::vector<std::string>& data) {
    for (size_t i = 0; i < data.size(); ++i) {
        // Stable key: metric_<0-based index>. Consumers read typed Values here;
        // the legacy positional reason map is no longer the contract.
        out.metrics["metric_" + std::to_string(i)] = ::hal::Value{data[i]};
    }
}

/// Result of a single test execution.
struct TestResult {
    std::string module_id;
    std::string test_id;
    std::string name;
    uint64_t sequence{0};
    std::chrono::system_clock::time_point timestamp;
    std::chrono::nanoseconds duration{0};
    TestOutput output;
    std::vector<TestResult> subtests;
};

} // namespace hal

// Bring canonical types into the global namespace for backward compatibility
// with all module headers and implementations.
using hal::TestResult;
using hal::TestOutput;
using hal::Status;
using hal::Severity;
using hal::Value;


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
 * @struct ModuleRunConfig
 * @brief Normalized per-run configuration delivered to a module by the Client.
 * @details Carries the common configuration fields shared by every module
 *          Config proto (hal.modules.<id>.Config) plus the selected test
 *          list. Module-specific config fields, when introduced, are carried
 *          by overriding configure() in the module class.
 */
struct ModuleRunConfig
{
    std::vector<std::string> test_names;  ///< Selected tests; empty = module default
    int      interval_ms     = 1000;      ///< Cycle interval (LONG_TERM / MONITORING)
    long long duration_ms    = 0;         ///< Total run time; 0 = run until stop()
    bool     run_forever     = false;     ///< Ignore duration_ms; run until stop()
    int      poll_interval_ms = 1000;     ///< Poll interval (EVENT_BASED)
    int      test_timeout_ms  = 30000;    ///< HAL-014: per-test watchdog timeout
};

/**
 * @class TestBase
 * @brief Base class for all testing classes
 * @details Provides four test modes:
 *   SINGLE_SHOT       — run all tests once (synchronous)
 *   MONITORING     — run indefinitely at a fixed interval (background thread)
 *   LONG_TERM      — run for a fixed total duration at a fixed interval (background thread)
 *   EVENT          — run continuously, fire a callback when criteria are met (background thread)
 *
 * For background modes, call stop() to abort early, or wait for isRunning() to return false.
 *
 * @threadSafety HAL-010 contract: TestBase instances are NOT thread-safe for
 * concurrent test execution. The background loop and each external caller
 * must be serialised by the owner (the Client adapter layer holds a
 * per-module mutex). Control APIs (stop/pause/resume) are safe to call from
 * any thread while a background loop is running; they only touch atomics.
 */
class TestBase
{
  public:
    TestBase();
    virtual ~TestBase();

    /**
     * @brief Run tests in SINGLE_SHOT or MONITORING mode (backward-compatible entry point)
     * @param mode        SINGLE_SHOT (synchronous) or MONITORING (starts background thread)
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
     * @brief Apply a per-run configuration delivered by the Client (HAL-001).
     * @details Stores the normalized config (test selection + common timing
     *          parameters) so runSelectedTests()/runAllTests() implementations
     *          can honour it via config(). Modules with extra config fields
     *          override this to decode them; the default stores the common
     *          surface.
     */
    virtual void configure(const ModuleRunConfig& config) { module_config_ = config; }

    /**
     * @brief The last configuration applied via configure()
     */
    const ModuleRunConfig& config() const { return module_config_; }

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
     * @brief Pause execution of an active background test loop.
     * The current cycle completes; the next cycle is held until resume() is called.
     * Has no effect on SINGLE_SHOT / SINGLE_SHOT tests (they are already synchronous).
     */
    virtual void pause();

    /**
     * @brief Resume a paused background test loop.
     */
    virtual void resume();

    /**
     * @brief Returns true while a background test thread is running
     */
    bool isRunning() const;

    /**
     * @brief Returns true if the test loop is paused (between cycles).
     */
    bool isPaused() const;

  protected:
    virtual std::vector<TestResult> runAllTests() = 0;
    virtual TestResult runSingleTest(const std::string& testName) = 0;

  private:
    // Exception-safe wrapper: any exception from runAllTests() is caught and
    // returned as a single ERROR TestResult instead of crashing the thread.
    std::vector<TestResult> runAllTestsSafe();

    // Block the calling thread while paused_ is set (honours stopped_ to exit early).
    void waitWhilePaused();

    // HAL-013: sleep until `deadline` (steady_clock), waking early on stop()
    // or pause()/resume() via the class condition variable instead of a
    // 50 ms polling loop.
    void sleepUntil(std::chrono::steady_clock::time_point deadline);

    std::atomic<bool> stopped_;
    std::atomic<bool> paused_{false};
    // HAL-003: number of test cycles currently executing (inside
    // runAllTestsSafe). The base destructor waits (bounded) for this to
    // reach zero before the derived class is fully destroyed, so a
    // background cycle can never touch a half-destroyed derived object.
    std::atomic<int> cycles_in_flight_{0};
    std::mutex wake_mu_;
    std::condition_variable wake_cv_;
    std::thread monitoring_thread_;

  protected:
    // HAL-001: configuration delivered by the Client (test selection + common
    // timing). Accessible to module implementations via config().
    ModuleRunConfig module_config_;
};

