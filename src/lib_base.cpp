/**
 * @file lib_base.cpp
 * @brief Implementation of TestBase class
 * @author sandesh ghimire
 * @copyright (C) Lucid Motors
 */

#include "lib_base.h"
#include <iostream>
#include <chrono>

TestBase::TestBase() : stopped_(false) {}

TestBase::~TestBase() {
    stop();
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
}

void TestBase::runTests(TestMode mode, std::function<void(const std::vector<TestResult>&)> callback, int interval_ms) {
    if (mode == TestMode::ONE_SHOT) {
        auto results = runAllTests();
        if (callback) {
            callback(results);
        }
    } else if (mode == TestMode::MONITORING) {
        stopped_ = false;
        monitoring_thread_ = std::thread([this, callback, interval_ms]() {
            while (!stopped_) {
                auto results = runAllTests();
                if (callback) {
                    callback(results);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
            }
        });
    }
}

void TestBase::stop() {
    stopped_ = true;
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
}

std::vector<TestResult> TestBase::runSelectedTests(const std::vector<std::string>& testNames) {
    std::vector<TestResult> results;
    for (const auto& testName : testNames) {
        results.push_back(runSingleTest(testName));
    }
    return results;
}