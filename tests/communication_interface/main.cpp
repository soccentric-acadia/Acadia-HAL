#include "lib_base.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "communication_interface_lib/communication_interface_lib.h"

std::atomic<bool> stop_flag(false);

void signal_handler(int signal)
{
    (void)signal;
    stop_flag = true;
}

int main(int argc, char *argv[])
{
    TestMode mode = TestMode::ONE_SHOT;
    if (argc > 1)
    {
        std::string arg = argv[1];
        if (arg == "monitoring")
        {
            mode = TestMode::MONITORING;
        }
    }

    auto printResults = [&](const std::vector<TestResult> &results) {
        if (mode == TestMode::MONITORING)
        {
            int ret = system("clear");
            (void)ret;
        }
        static int cycle = 0;
        cycle++;
        std::cout << "\nTest Results (Cycle " << cycle << "):\n";
        std::cout << std::left << std::setw(40) << "Test Name" << std::setw(10) << "Result" << std::setw(15)
                  << "Duration (s)" << std::endl;
        std::cout << std::string(65, '-') << std::endl;
        int passed = 0;
        int failed = 0;
        std::chrono::duration<double> totalTime = std::chrono::duration<double>::zero();
        for (const auto &res : results)
        {
            std::cout << std::left << std::setw(40) << res.name << std::setw(10)
                      << (res.output.passed ? "PASS" : "FAIL") << std::setw(15) << std::fixed << std::setprecision(6)
                      << res.duration.count() << std::endl;
            if (res.output.passed)
                passed++;
            else
                failed++;
            totalTime += res.duration;
        }
        std::cout << std::string(65, '-') << std::endl;
        std::cout << "Total Tests Run: " << results.size() << std::endl;
        std::cout << "Passed: " << passed << std::endl;
        std::cout << "Failed: " << failed << std::endl;
        std::cout << "Total Time: " << std::fixed << std::setprecision(6) << totalTime.count() << " seconds"
                  << std::endl;
    };

    CommunicationInterfaceTesting tester;
    std::cout << "CommunicationInterfaceTesting test started in "
              << (mode == TestMode::ONE_SHOT ? "ONE_SHOT" : "MONITORING") << " mode." << std::endl;

    if (mode == TestMode::MONITORING)
    {
        std::signal(SIGINT, signal_handler);
        std::cout << "Press Ctrl+C to stop monitoring." << std::endl;
    }

    // Option 1: Run ALL tests (default behavior)

    tester.runTests(mode, printResults);

    // Option 2: Run SELECTED tests in any order

    // Uncomment the lines below and comment out the line above to run specific tests

    /*


    std::vector<std::string> selectedTests = {


        // Add test names here - see getAvailableTests() for full list


        // Example: Select any tests in any order


    };


    auto results = tester.runSelectedTests(selectedTests);


    printResults(results);


    */

    // Option 3: Run individual tests one by one

    // Uncomment to use this approach

    /*


    std::vector<TestResult> results;


    // Add individual test calls here


    // Example: results.push_back(tester.runSingleTest("testName"));


    printResults(results);


    */

    if (mode == TestMode::MONITORING)
    {
        // Wait for stop signal
        while (!stop_flag)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        tester.stop();
        std::cout << "Monitoring stopped." << std::endl;
    }

    std::cout << "CommunicationInterfaceTesting test completed." << std::endl;
    return 0;
}