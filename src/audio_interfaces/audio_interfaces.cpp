/**
 * @file audio_interfaces.cpp
 * @brief Implementation file for AudioInterfaces class
 * @author sandesh ghimire
 * @copyright (C) Lucid Motors
 */

#include "audio_interfaces.h"

/**
 * @brief Constructor for AudioInterfaces
 */
AudioInterfaces::AudioInterfaces()
{
}

/**
 * @brief Destructor for AudioInterfaces
 */
AudioInterfaces::~AudioInterfaces()
{
}

/**
 * @brief Implementation of testI2sInterface
 * @return TestOutput with pass/fail and data
 */
TestOutput AudioInterfaces::testI2sInterface()
{
    return {true, {}};  // placeholder implementation
}

/**
 * @brief Implementation of testAudioInput
 * @return TestOutput with pass/fail and data
 */
TestOutput AudioInterfaces::testAudioInput()
{
    return {true, {}};  // placeholder implementation
}

/**
 * @brief Implementation of testAudioOutput
 * @return TestOutput with pass/fail and data
 */
TestOutput AudioInterfaces::testAudioOutput()
{
    return {true, {}};  // placeholder implementation
}

/**
 * @brief Implementation of testAudioSampleRates
 * @return TestOutput with pass/fail and data
 */
TestOutput AudioInterfaces::testAudioSampleRates()
{
    return {true, {}};  // placeholder implementation
}

/**
 * @brief Implementation of testAudioBitDepths
 * @return TestOutput with pass/fail and data
 */
TestOutput AudioInterfaces::testAudioBitDepths()
{
    return {true, {}};  // placeholder implementation
}

/**
 * @brief Implementation of testMultiChannelAudio
 * @return TestOutput with pass/fail and data
 */
TestOutput AudioInterfaces::testMultiChannelAudio()
{
    return {true, {}};  // placeholder implementation
}

/**
 * @brief Implementation of testAudioDmaTransfers
 * @return TestOutput with pass/fail and data
 */
TestOutput AudioInterfaces::testAudioDmaTransfers()
{
    return {true, {}};  // placeholder implementation
}

/**
 * @brief Implementation of testAudioLatency
 * @return TestOutput with pass/fail and data
 */
TestOutput AudioInterfaces::testAudioLatency()
{
    return {true, {"latency: 5ms", "jitter: 0.1ms"}};  // placeholder implementation with sample data
}
std::vector<TestResult> AudioInterfaces::runAllTests()
{
    std::vector<TestResult> results;

    // Run testI2sInterface
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testI2sInterface();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testI2sInterface", result, duration});
    }

    // Run testAudioInput
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testAudioInput();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testAudioInput", result, duration});
    }

    // Run testAudioOutput
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testAudioOutput();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testAudioOutput", result, duration});
    }

    // Run testAudioSampleRates
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testAudioSampleRates();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testAudioSampleRates", result, duration});
    }

    // Run testAudioBitDepths
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testAudioBitDepths();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testAudioBitDepths", result, duration});
    }

    // Run testMultiChannelAudio
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testMultiChannelAudio();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testMultiChannelAudio", result, duration});
    }

    // Run testAudioDmaTransfers
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testAudioDmaTransfers();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testAudioDmaTransfers", result, duration});
    }

    // Run testAudioLatency
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testAudioLatency();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testAudioLatency", result, duration});
    }

    return results;
}

/**
 * @brief Get list of all available test names
 * @return Vector of test names
 */
std::vector<std::string> AudioInterfaces::getAvailableTests() {
    return {
        "testI2sInterface",
        "testAudioInput",
        "testAudioOutput",
        "testAudioSampleRates",
        "testAudioBitDepths",
        "testMultiChannelAudio",
        "testAudioDmaTransfers",
        "testAudioLatency",
    };
}

/**
 * @brief Run a single test by name
 * @param testName Name of the test to run
 * @return Test result with timing
 */
TestResult AudioInterfaces::runSingleTest(const std::string& testName) {
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput result = {false, {}};

    if (testName == "testI2sInterface") {
        result = testI2sInterface();
    } else if (testName == "testAudioInput") {
        result = testAudioInput();
    } else if (testName == "testAudioOutput") {
        result = testAudioOutput();
    } else if (testName == "testAudioSampleRates") {
        result = testAudioSampleRates();
    } else if (testName == "testAudioBitDepths") {
        result = testAudioBitDepths();
    } else if (testName == "testMultiChannelAudio") {
        result = testMultiChannelAudio();
    } else if (testName == "testAudioDmaTransfers") {
        result = testAudioDmaTransfers();
    } else if (testName == "testAudioLatency") {
        result = testAudioLatency();
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
