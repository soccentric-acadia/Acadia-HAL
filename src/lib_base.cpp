/**
 * @file lib_base.cpp
 * @brief Implementation of TestBase class
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */

#include "lib_base.h"
#include "platform_rpi5.h"
#include <chrono>
#include <iostream>

TestBase::TestBase() : stopped_(true)
{
}

TestBase::~TestBase()
{
    stop();
}

// ---------------------------------------------------------------------------
// ONE_SHOT / MONITORING (backward-compatible)
// ---------------------------------------------------------------------------

void TestBase::runTests(TestMode mode,
                        std::function<void(const std::vector<TestResult>&)> callback,
                        int interval_ms)
{
    if (mode == TestMode::ONE_SHOT) {
        auto results = runAllTests();
        if (callback) {
            callback(results);
        }
    } else if (mode == TestMode::MONITORING) {
        stop();
        stopped_ = false;
        monitoring_thread_ = std::thread([this, callback, interval_ms]() {
            while (!stopped_) {
                auto results = runAllTests();
                if (callback) {
                    callback(results);
                }
                auto wake = std::chrono::steady_clock::now() + std::chrono::milliseconds(interval_ms);
                while (!stopped_ && std::chrono::steady_clock::now() < wake) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            }
        });
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
            if (duration_ms > 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();
                if (elapsed >= duration_ms) {
                    break;
                }
            }
            auto results = runAllTests();
            ++cycle;
            if (callback) {
                callback(results, cycle);
            }
            auto wake = std::chrono::steady_clock::now() + std::chrono::milliseconds(interval_ms);
            while (!stopped_ && std::chrono::steady_clock::now() < wake) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
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
            snap.testResults = runAllTests();
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
            auto wake = std::chrono::steady_clock::now() + std::chrono::milliseconds(interval_ms);
            while (!stopped_ && std::chrono::steady_clock::now() < wake) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
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
            if (max_duration_ms > 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();
                if (elapsed >= max_duration_ms) {
                    break;
                }
            }
            auto results = runAllTests();
            if (criteria.predicate && criteria.predicate(results)) {
                if (event_callback) {
                    event_callback(results);
                }
            }
            auto wake = std::chrono::steady_clock::now() + std::chrono::milliseconds(poll_interval_ms);
            while (!stopped_ && std::chrono::steady_clock::now() < wake) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
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
    if (monitoring_thread_.joinable()) {
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
}

void TestBase::resume()
{
    paused_.store(false);
}

bool TestBase::isPaused() const
{
    return paused_.load();
}

std::vector<TestResult> TestBase::runSelectedTests(const std::vector<std::string>& testNames)
{
    std::vector<TestResult> results;
    for (const auto& name : testNames) {
        results.push_back(runSingleTest(name));
    }
    return results;
}