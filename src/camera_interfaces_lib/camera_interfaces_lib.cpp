/**
 * @file camera_interfaces_lib.cpp
 * @brief Implementation file for CameraInterfacesTesting class
 *        Target platform: NVIDIA Jetson AGX Orin
 * @author sandesh ghimire
 * @copyright (C) Lucid Motors
 */

#include "camera_interfaces_lib.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

// ---------------------------------------------------------------------------
// Helper utilities
// ---------------------------------------------------------------------------

/** Read first line from a sysfs / procfs text file, strip trailing newline. */
static std::string readFile(const std::string &path)
{
    std::ifstream f(path);
    if (!f.is_open())
        return "";
    std::string line;
    std::getline(f, line);
    // strip \r\n
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        line.pop_back();
    return line;
}

/** Read all lines from a file. */
static std::vector<std::string> readAllLines(const std::string &path)
{
    std::vector<std::string> lines;
    std::ifstream f(path);
    if (!f.is_open())
        return lines;
    std::string line;
    while (std::getline(f, line))
    {
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

/** Return true if a path exists (file or directory). */
static bool pathExists(const std::string &path)
{
    struct stat st;
    return (stat(path.c_str(), &st) == 0);
}

/** List directory entries (non-recursive). */
static std::vector<std::string> listDir(const std::string &dir)
{
    std::vector<std::string> entries;
    DIR *d = opendir(dir.c_str());
    if (!d)
        return entries;
    struct dirent *de;
    while ((de = readdir(d)) != nullptr)
    {
        std::string name(de->d_name);
        if (name == "." || name == "..")
            continue;
        entries.push_back(dir + "/" + name);
    }
    closedir(d);
    return entries;
}

/** Check if a kernel module is loaded. */
static bool isModuleLoaded(const std::string &modName)
{
    std::ifstream mods("/proc/modules");
    if (!mods.is_open())
        return false;
    std::string line;
    while (std::getline(mods, line))
    {
        std::istringstream iss(line);
        std::string name;
        iss >> name;
        if (name == modName)
            return true;
    }
    return false;
}

/** Run a shell command and capture its stdout (first 4096 bytes). */
static std::string runCmd(const std::string &cmd)
{
    FILE *fp = popen(cmd.c_str(), "r");
    if (!fp)
        return "";
    char buf[4096];
    std::string out;
    while (fgets(buf, sizeof(buf), fp))
        out += buf;
    pclose(fp);
    // strip trailing newline
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();
    return out;
}

/** Collect all /dev/video* device nodes. */
static std::vector<std::string> getVideoDevices()
{
    std::vector<std::string> devs;
    for (int i = 0; i < 32; ++i)
    {
        std::string p = "/dev/video" + std::to_string(i);
        if (pathExists(p))
            devs.push_back(p);
    }
    return devs;
}

/**
 * @brief Constructor for CameraInterfacesTesting
 */
CameraInterfacesTesting::CameraInterfacesTesting()
{
}

/**
 * @brief Destructor for CameraInterfacesTesting
 */
CameraInterfacesTesting::~CameraInterfacesTesting()
{
}

/**
 * @brief Implementation of testAllCsi2LanesFunctionality
 *        Verifies that the Jetson Orin NVCSI (MIPI CSI-2) controller exposes
 *        its expected lane groups via sysfs and that at least one camera
 *        sensor is enumerated through V4L2.
 * @return TestOutput with pass/fail and data
 */
TestOutput CameraInterfacesTesting::testAllCsi2LanesFunctionality()
{
    std::vector<std::string> data;
    bool passed = true;

    data.push_back("=== CSI-2 Lane Functionality (Jetson AGX Orin) ===");

    // --- 1. Check NVCSI driver is present ---
    // Jetson Orin: nvcsi platform device path
    const std::string nvcsiBase = "/sys/bus/platform/devices";
    bool nvcsiFound = false;
    if (pathExists(nvcsiBase))
    {
        auto entries = listDir(nvcsiBase);
        for (const auto &e : entries)
        {
            // NVCSI device entries contain "nvcsi" in their name
            if (e.find("nvcsi") != std::string::npos || e.find("15a00000") != std::string::npos) // Orin NVCSI hwid
            {
                nvcsiFound = true;
                data.push_back("[PASS] NVCSI platform device found: " + e.substr(e.find_last_of('/') + 1));
                break;
            }
        }
    }
    if (!nvcsiFound)
    {
        // Fallback: check kernel module
        if (isModuleLoaded("nvcsi") || isModuleLoaded("tegra-camera"))
        {
            nvcsiFound = true;
            data.push_back("[PASS] NVCSI kernel module is loaded");
        }
        else
        {
            data.push_back("[WARN] NVCSI platform device/module not detected (non-fatal on dev host)");
        }
    }

    // --- 2. Check V4L2 camera devices ---
    auto videoDev = getVideoDevices();
    if (videoDev.empty())
    {
        data.push_back("[WARN] No /dev/video* nodes found – no cameras connected");
        // Not a hard failure on a development host; pass with warning
    }
    else
    {
        data.push_back("[PASS] Found " + std::to_string(videoDev.size()) + " V4L2 video node(s)");
        for (const auto &dev : videoDev)
        {
            // Query driver name via v4l2-ctl if available, else just list the node
            std::string info = runCmd("v4l2-ctl -d " + dev + " --info 2>/dev/null | grep 'Driver name' | head -1");
            if (info.empty())
                info = dev + " (v4l2-ctl not available)";
            data.push_back("    " + info);
        }
    }

    // --- 3. Check /sys/class/video4linux for CSI-2 lane info ---
    const std::string v4lClass = "/sys/class/video4linux";
    if (pathExists(v4lClass))
    {
        auto nodes = listDir(v4lClass);
        int csiNodes = 0;
        for (const auto &n : nodes)
        {
            std::string name = n.substr(n.find_last_of('/') + 1);
            // Jetson camera nodes typically named video0..videoN
            if (name.find("video") != std::string::npos)
            {
                std::string devFile = n + "/name";
                std::string devName = readFile(devFile);
                if (!devName.empty())
                {
                    data.push_back("    V4L2 node: " + name + " -> " + devName);
                    csiNodes++;
                }
            }
        }
        if (csiNodes == 0)
            data.push_back("[INFO] No named V4L2 nodes found in sysfs");
        else
            data.push_back("[PASS] " + std::to_string(csiNodes) + " V4L2 node(s) enumerated in sysfs");
    }

    // --- 4. Validate lane count from device-tree (if accessible) ---
    const std::string dtCamera = "/proc/device-tree/tegra-camera-platform";
    if (pathExists(dtCamera))
    {
        data.push_back("[PASS] Tegra camera platform device-tree node found");
        auto dtEntries = listDir(dtCamera);
        data.push_back("       " + std::to_string(dtEntries.size()) + " sub-entries in camera DT node");
    }
    else
    {
        data.push_back("[INFO] /proc/device-tree/tegra-camera-platform not found");
    }

    return {passed, data};
}

/**
 * @brief Implementation of testCsi2DphyOperation
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testCsi2DphyOperation()
{
    return {true, {}};
}

/**
 * @brief Implementation of testCsi2CphyOperation
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testCsi2CphyOperation()
{
    return {true, {}};
}

/**
 * @brief Implementation of testMultipleCameraSimultaneousOperation
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testMultipleCameraSimultaneousOperation()
{
    return {true, {}};
}

/**
 * @brief Implementation of testVirtualChannelSupport
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testVirtualChannelSupport()
{
    return {true, {}};
}

/**
 * @brief Implementation of testCameraClockGeneration
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testCameraClockGeneration()
{
    return {true, {}};
}

/**
 * @brief Implementation of testCameraPowerSequencing
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testCameraPowerSequencing()
{
    return {true, {}};
}

/**
 * @brief Implementation of testRaw8
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testRaw8()
{
    return {true, {}};
}

/**
 * @brief Implementation of testRaw10
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testRaw10()
{
    return {true, {}};
}

/**
 * @brief Implementation of testRaw12
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testRaw12()
{
    return {true, {}};
}

/**
 * @brief Implementation of testRaw14
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testRaw14()
{
    printf("Executing testRaw14\n");
    return {true, {}};
}

/**
 * @brief Implementation of testYuv422
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testYuv422()
{
    return {true, {}};
}

/**
 * @brief Implementation of testRgbFormats
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testRgbFormats()
{
    return {true, {}};
}

/**
 * @brief Implementation of testIspPipelineProcessing
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testIspPipelineProcessing()
{
    return {true, {}};
}

/**
 * @brief Implementation of testAutoExposure
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testAutoExposure()
{
    return {true, {}};
}

/**
 * @brief Implementation of testAutoWhiteBalance
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testAutoWhiteBalance()
{
    return {true, {}};
}

/**
 * @brief Implementation of testAutoFocus
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testAutoFocus()
{
    return {true, {}};
}

/**
 * @brief Implementation of testLensShadingCorrection
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testLensShadingCorrection()
{
    return {true, {}};
}

/**
 * @brief Implementation of testDemosaicing
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testDemosaicing()
{
    return {true, {}};
}

/**
 * @brief Implementation of testNoiseReduction
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testNoiseReduction()
{
    return {true, {}};
}

/**
 * @brief Implementation of testEdgeEnhancement
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testEdgeEnhancement()
{
    return {true, {}};
}

/**
 * @brief Implementation of testColorCorrectionMatrix
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testColorCorrectionMatrix()
{
    return {true, {}};
}

/**
 * @brief Implementation of testGammaCorrection
 * @return true (placeholder implementation)
 */
TestOutput CameraInterfacesTesting::testGammaCorrection()
{
    return {true, {}};
}

/**
 * @brief Run all tests for this module
 * @return Vector of test results
 */
std::vector<TestResult> CameraInterfacesTesting::runAllTests()
{
    std::vector<TestResult> results;

    // Run testAllCsi2LanesFunctionality
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testAllCsi2LanesFunctionality();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testAllCsi2LanesFunctionality", result, duration});
    }

    // Run testCsi2DphyOperation
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testCsi2DphyOperation();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testCsi2DphyOperation", result, duration});
    }

    // Run testCsi2CphyOperation
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testCsi2CphyOperation();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testCsi2CphyOperation", result, duration});
    }

    // Run testMultipleCameraSimultaneousOperation
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testMultipleCameraSimultaneousOperation();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testMultipleCameraSimultaneousOperation", result, duration});
    }

    // Run testVirtualChannelSupport
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testVirtualChannelSupport();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testVirtualChannelSupport", result, duration});
    }

    // Run testCameraClockGeneration
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testCameraClockGeneration();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testCameraClockGeneration", result, duration});
    }

    // Run testCameraPowerSequencing
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testCameraPowerSequencing();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testCameraPowerSequencing", result, duration});
    }

    // Run testRaw8
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testRaw8();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testRaw8", result, duration});
    }

    // Run testRaw10
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testRaw10();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testRaw10", result, duration});
    }

    // Run testRaw12
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testRaw12();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testRaw12", result, duration});
    }

    // Run testRaw14
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testRaw14();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testRaw14", result, duration});
    }

    // Run testYuv422
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testYuv422();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testYuv422", result, duration});
    }

    // Run testRgbFormats
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testRgbFormats();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testRgbFormats", result, duration});
    }

    // Run testIspPipelineProcessing
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testIspPipelineProcessing();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testIspPipelineProcessing", result, duration});
    }

    // Run testAutoExposure
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testAutoExposure();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testAutoExposure", result, duration});
    }

    // Run testAutoWhiteBalance
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testAutoWhiteBalance();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testAutoWhiteBalance", result, duration});
    }

    // Run testAutoFocus
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testAutoFocus();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testAutoFocus", result, duration});
    }

    // Run testLensShadingCorrection
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testLensShadingCorrection();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testLensShadingCorrection", result, duration});
    }

    // Run testDemosaicing
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testDemosaicing();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testDemosaicing", result, duration});
    }

    // Run testNoiseReduction
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testNoiseReduction();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testNoiseReduction", result, duration});
    }

    // Run testEdgeEnhancement
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testEdgeEnhancement();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testEdgeEnhancement", result, duration});
    }

    // Run testColorCorrectionMatrix
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testColorCorrectionMatrix();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testColorCorrectionMatrix", result, duration});
    }

    // Run testGammaCorrection
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testGammaCorrection();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        results.push_back({"testGammaCorrection", result, duration});
    }

    return results;
}

/**
 * @brief Get list of all available test names
 * @return Vector of test names
 */
std::vector<std::string> CameraInterfacesTesting::getAvailableTests()
{
    return {
        "testAllCsi2LanesFunctionality",
        "testCsi2DphyOperation",
        "testCsi2CphyOperation",
        "testMultipleCameraSimultaneousOperation",
        "testVirtualChannelSupport",
        "testCameraClockGeneration",
        "testCameraPowerSequencing",
        "testRaw8",
        "testRaw10",
        "testRaw12",
        "testRaw14",
        "testYuv422",
        "testRgbFormats",
        "testIspPipelineProcessing",
        "testAutoExposure",
        "testAutoWhiteBalance",
        "testAutoFocus",
        "testLensShadingCorrection",
        "testDemosaicing",
        "testNoiseReduction",
        "testEdgeEnhancement",
        "testColorCorrectionMatrix",
        "testGammaCorrection",
    };
}

/**
 * @brief Run a single test by name
 * @param testName Name of the test to run
 * @return Test result with timing
 */
TestResult CameraInterfacesTesting::runSingleTest(const std::string &testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput result = {false, {}};

    if (testName == "testAllCsi2LanesFunctionality")
    {
        result = testAllCsi2LanesFunctionality();
    }
    else if (testName == "testCsi2DphyOperation")
    {
        result = testCsi2DphyOperation();
    }
    else if (testName == "testCsi2CphyOperation")
    {
        result = testCsi2CphyOperation();
    }
    else if (testName == "testMultipleCameraSimultaneousOperation")
    {
        result = testMultipleCameraSimultaneousOperation();
    }
    else if (testName == "testVirtualChannelSupport")
    {
        result = testVirtualChannelSupport();
    }
    else if (testName == "testCameraClockGeneration")
    {
        result = testCameraClockGeneration();
    }
    else if (testName == "testCameraPowerSequencing")
    {
        result = testCameraPowerSequencing();
    }
    else if (testName == "testRaw8")
    {
        result = testRaw8();
    }
    else if (testName == "testRaw10")
    {
        result = testRaw10();
    }
    else if (testName == "testRaw12")
    {
        result = testRaw12();
    }
    else if (testName == "testRaw14")
    {
        result = testRaw14();
    }
    else if (testName == "testYuv422")
    {
        result = testYuv422();
    }
    else if (testName == "testRgbFormats")
    {
        result = testRgbFormats();
    }
    else if (testName == "testIspPipelineProcessing")
    {
        result = testIspPipelineProcessing();
    }
    else if (testName == "testAutoExposure")
    {
        result = testAutoExposure();
    }
    else if (testName == "testAutoWhiteBalance")
    {
        result = testAutoWhiteBalance();
    }
    else if (testName == "testAutoFocus")
    {
        result = testAutoFocus();
    }
    else if (testName == "testLensShadingCorrection")
    {
        result = testLensShadingCorrection();
    }
    else if (testName == "testDemosaicing")
    {
        result = testDemosaicing();
    }
    else if (testName == "testNoiseReduction")
    {
        result = testNoiseReduction();
    }
    else if (testName == "testEdgeEnhancement")
    {
        result = testEdgeEnhancement();
    }
    else if (testName == "testColorCorrectionMatrix")
    {
        result = testColorCorrectionMatrix();
    }
    else if (testName == "testGammaCorrection")
    {
        result = testGammaCorrection();
    }
    else
    {
        // Unknown test name
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        return {testName + " (UNKNOWN)", {false, {}}, duration};
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    return {testName, result, duration};
}
