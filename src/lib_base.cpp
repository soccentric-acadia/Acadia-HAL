/**
 * @file lib_base.cpp
 * @brief Implementation of TestBase class
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */

#include "lib_base.h"
#include "platform_rpi5.h"
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <unordered_set>

TestBase::TestBase() : stopped_(true)
{
}

TestBase::~TestBase()
{
    // HAL-003: the background thread must never touch virtual functions of
    // the derived class after it begins destruction. Signal the loop to stop
    // and wait (bounded) for any in-flight test cycle to complete before the
    // derived part of the object disappears.
    stopped_.store(true);
    wake_cv_.notify_all();
    for (int ms = 0; ms < 3000 && cycles_in_flight_.load() > 0; ms += 10) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    stop();
}

// ---------------------------------------------------------------------------
// Internal: exception-safe wrapper around runAllTests().
// Returns a single ERROR result if any exception escapes the derived class.
// ---------------------------------------------------------------------------
std::vector<TestResult> TestBase::runAllTestsSafe()
{
    ++cycles_in_flight_;
    struct InFlightGuard {
        std::atomic<int>& c;
        ~InFlightGuard() { --c; }
    } guard{cycles_in_flight_};
    try {
        return runAllTests();
    } catch (const std::exception& ex) {
        TestResult err;
        err.name    = "runAllTests_exception";
        err.test_id = err.name;
        err.output.status        = hal::Status::ERROR;
        err.output.severity      = hal::Severity::CRITICAL;
        err.output.error_message = std::string("Exception in runAllTests: ") + ex.what();
        std::cerr << "[TestBase] " << err.output.error_message << "\n";
        return {err};
    } catch (...) {
        TestResult err;
        err.name    = "runAllTests_exception";
        err.test_id = err.name;
        err.output.status        = hal::Status::ERROR;
        err.output.severity      = hal::Severity::CRITICAL;
        err.output.error_message = "Unknown exception in runAllTests";
        std::cerr << "[TestBase] " << err.output.error_message << "\n";
        return {err};
    }
}

// Internal: wait while paused (checked at the start of each background cycle).
// HAL-013: uses the class condition variable instead of a 50 ms polling loop.
void TestBase::waitWhilePaused()
{
    std::unique_lock<std::mutex> lk(wake_mu_);
    wake_cv_.wait(lk, [this] { return !paused_.load() || stopped_.load(); });
}

// HAL-013: sleep until `deadline`, waking early on stop()/pause()/resume().
void TestBase::sleepUntil(std::chrono::steady_clock::time_point deadline)
{
    std::unique_lock<std::mutex> lk(wake_mu_);
    wake_cv_.wait_until(lk, deadline, [this] {
        return stopped_.load();
    });
}

// ---------------------------------------------------------------------------
// SINGLE_SHOT / MONITORING (backward-compatible)
// ---------------------------------------------------------------------------

void TestBase::runTests(TestMode mode,
                        std::function<void(const std::vector<TestResult>&)> callback,
                        int interval_ms)
{
    if (mode == TestMode::SINGLE_SHOT) {
        auto results = runAllTestsSafe();
        if (callback) {
            callback(results);
        }
    } else if (mode == TestMode::MONITORING) {
        stop();
        stopped_ = false;
        monitoring_thread_ = std::thread([this, callback, interval_ms]() {
            while (!stopped_) {
                waitWhilePaused();
                if (stopped_) break;
                auto results = runAllTestsSafe();
                if (callback) {
                    callback(results);
                }
                sleepUntil(std::chrono::steady_clock::now() + std::chrono::milliseconds(interval_ms));
            }
        });
    } else {
        // HAL-005: never silently ignore LONG_TERM / EVENT_BASED here.
        std::cerr << "[TestBase] runTests(): mode " << static_cast<int>(mode)
                  << " is not supported by this entry point — use runLongTerm() "
                     "or runEventTest() instead. No tests were run.\n";
    }
}

// ---------------------------------------------------------------------------
// LONG_TERM — fixed-duration periodic test
// ---------------------------------------------------------------------------

void TestBase::runLongTerm(int interval_ms, long long duration_ms,
                            std::function<void(const std::vector<TestResult>&, int)> callback)
{
    stop();
    stopped_ = false;
    monitoring_thread_ = std::thread([this, interval_ms, duration_ms, callback]() {
        auto start = std::chrono::steady_clock::now();
        int cycle = 0;
        while (!stopped_) {
            waitWhilePaused();
            if (stopped_) break;
            if (duration_ms > 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();
                if (elapsed >= duration_ms) {
                    break;
                }
            }
            auto results = runAllTestsSafe();
            ++cycle;
            if (callback) {
                callback(results, cycle);
            }
            sleepUntil(std::chrono::steady_clock::now() + std::chrono::milliseconds(interval_ms));
        }
        stopped_ = true;
    });
}

// ---------------------------------------------------------------------------
// MONITORING TEST — long-term + hardware monitor sampling
// ---------------------------------------------------------------------------

void TestBase::runMonitoringTest(int interval_ms, long long duration_ms,
                                  const std::vector<MonitorItem>& monitorItems,
                                  std::function<void(const MonitoringSnapshot&)> callback)
{
    stop();
    stopped_ = false;
    monitoring_thread_ = std::thread([this, interval_ms, duration_ms, monitorItems, callback]() {
        auto start = std::chrono::steady_clock::now();
        int cycle = 0;
        while (!stopped_) {
            waitWhilePaused();
            if (stopped_) break;
            if (duration_ms > 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();
                if (elapsed >= duration_ms) {
                    break;
                }
            }
            MonitoringSnapshot snap;
            snap.cycle = ++cycle;
            snap.timestamp = std::chrono::system_clock::now();
            snap.testResults = runAllTestsSafe();
            for (const auto& item : monitorItems) {
                std::string val;
                try {
                    val = item.reader();
                } catch (...) {
                    val = "error";
                }
                snap.monitorData.emplace_back(item.name, val);
            }
            if (callback) {
                callback(snap);
            }
            sleepUntil(std::chrono::steady_clock::now() + std::chrono::milliseconds(interval_ms));
        }
        stopped_ = true;
    });
}

// ---------------------------------------------------------------------------
// EVENT TEST — fire callback when predicate is satisfied
// ---------------------------------------------------------------------------

void TestBase::runEventTest(int poll_interval_ms, const EventCriteria& criteria,
                             std::function<void(const std::vector<TestResult>&)> event_callback,
                             long long max_duration_ms)
{
    stop();
    stopped_ = false;
    monitoring_thread_ = std::thread([this, poll_interval_ms, max_duration_ms,
                                       criteria, event_callback]() {
        auto start = std::chrono::steady_clock::now();
        while (!stopped_) {
            waitWhilePaused();
            if (stopped_) break;
            if (max_duration_ms > 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();
                if (elapsed >= max_duration_ms) {
                    break;
                }
            }
            auto results = runAllTestsSafe();
            if (criteria.predicate && criteria.predicate(results)) {
                if (event_callback) {
                    event_callback(results);
                }
            }
            sleepUntil(std::chrono::steady_clock::now() + std::chrono::milliseconds(poll_interval_ms));
        }
        stopped_ = true;
    });
}

// ---------------------------------------------------------------------------
// Control
// ---------------------------------------------------------------------------

void TestBase::stop()
{
    stopped_ = true;
    wake_cv_.notify_all();
    if (monitoring_thread_.joinable()) {
        // HAL-004: joining from inside a callback (the background thread
        // itself) throws std::system_error(resource_deadlock_would_occur).
        // In that case set stopped_ (done above) and detach — the loop
        // observes stopped_ and exits on its own; the joining thread's
        // control path never waits on itself.
        if (monitoring_thread_.get_id() == std::this_thread::get_id()) {
            monitoring_thread_.detach();
            return;
        }
        monitoring_thread_.join();
    }
}

bool TestBase::isRunning() const
{
    return !stopped_;
}

void TestBase::pause()
{
    paused_.store(true);
    wake_cv_.notify_all();
}

void TestBase::resume()
{
    paused_.store(false);
    wake_cv_.notify_all();
}

bool TestBase::isPaused() const
{
    return paused_.load();
}

std::vector<TestResult> TestBase::runSelectedTests(const std::vector<std::string>& testNames)
{
    const auto available = getAvailableTests();
    // HAL-008: hash-set membership instead of an O(n*m) linear scan.
    std::unordered_set<std::string> availableSet(available.begin(), available.end());

    const auto deadlineMs = static_cast<long long>(config().test_timeout_ms);

    std::vector<TestResult> results;
    results.reserve(testNames.size());
    for (const auto& name : testNames) {
        if (availableSet.find(name) == availableSet.end()) {
            TestResult err;
            err.name    = name;
            err.test_id = name;
            err.output.status        = hal::Status::ERROR;
            err.output.severity      = hal::Severity::ERROR;
            err.output.error_message = "Unknown test name: \"" + name + "\"";
            results.push_back(std::move(err));
            continue;
        }
        // HAL-014: per-test watchdog. The test runs on a worker thread; if it
        // exceeds config().test_timeout_ms the result is TIMEOUT and the
        // worker is abandoned (documented: a hung test thread is left
        // detached; the Client orchestrator enforces process-level limits).
        TestResult result;
        std::promise<bool> done;
        std::future<bool> doneF = done.get_future();
        std::thread worker([&]() {
            try {
                result = runSingleTest(name);
            } catch (const std::exception& ex) {
                TestResult err;
                err.name    = name;
                err.test_id = name;
                err.output.status        = hal::Status::ERROR;
                err.output.severity      = hal::Severity::CRITICAL;
                err.output.error_message = std::string("Exception in runSingleTest(\"") + name + "\"): " + ex.what();
                result = std::move(err);
            } catch (...) {
                TestResult err;
                err.name    = name;
                err.test_id = name;
                err.output.status        = hal::Status::ERROR;
                err.output.severity      = hal::Severity::CRITICAL;
                err.output.error_message = "Unknown exception in runSingleTest(\"" + name + "\")";
                result = std::move(err);
            }
            done.set_value(true);
        });
        if (doneF.wait_for(std::chrono::milliseconds(deadlineMs)) == std::future_status::ready) {
            worker.join();
        } else {
            // Timeout: abandon the worker (hung test). The detached worker
            // may still complete later; its result is discarded. Documented
            // trade-off — the Client-level watchdog bounds the run.
            worker.detach();
            TestResult err;
            err.name    = name;
            err.test_id = name;
            err.output.status        = hal::Status::TIMEOUT;
            err.output.severity      = hal::Severity::ERROR;
            err.output.error_message = "Test exceeded " + std::to_string(deadlineMs) + " ms per-test watchdog";
            result = std::move(err);
        }
        // HAL-007: stamp execution timestamp at the base level (modules no
        // longer need to set it). module_id remains adapter-owned.
        result.timestamp = std::chrono::system_clock::now();
        results.push_back(std::move(result));
    }
    return results;
}