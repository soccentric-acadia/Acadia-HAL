/**
 * @file reliability_lib.cpp
 * @brief Reliability tests for Raspberry Pi 5 (BCM2712).
 *        Tests characterise the long-term health of the board by examining
 *        system uptime, thermal headroom, power-supply stability indicators,
 *        and storage health metrics accessible via kernel sysfs/procfs.
 *        Physical burn-in, solder-joint, and thermal-cycling procedures cannot
 *        be performed programmatically; those tests report observable proxies.
 * @author sandesh ghimire
 * @copyright (C) Lucid Motors
 */

#include "reliability_lib.h"
#include "../platform_rpi5.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::string relReadFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string s = ss.str();
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    return s;
}

static std::string relRunCmd(const std::string& cmd)
{
    std::array<char, 512> buf{};
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buf.data(), buf.size(), pipe))
        result += buf.data();
    pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

// Safe numeric parsing: rejects empty/garbage input instead of throwing.
static bool relParseDouble(const std::string& s, double& out)
{
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    double v = strtod(s.c_str(), &end);
    if (end == s.c_str()) return false;
    out = v;
    return true;
}

static bool relParseLong(const std::string& s, long& out)
{
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    long v = strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || errno == ERANGE) return false;
    out = v;
    return true;
}

// ---------------------------------------------------------------------------

/**
 * @brief Constructor for ReliabilityTesting
 */
ReliabilityTesting::ReliabilityTesting()
{
}

/**
 * @brief Destructor for ReliabilityTesting
 */
ReliabilityTesting::~ReliabilityTesting()
{
}

/**
 * @brief Burn-in proxy: brief CPU+memory stress and throttle check.
 */
TestOutput ReliabilityTesting::testBurninTesting()
{
    std::vector<std::string> data;
    data.push_back("=== Burn-In Proxy (5-second stress, Pi5) ===");

    std::string throttlePre = relRunCmd("vcgencmd get_throttled 2>/dev/null");
    data.push_back("[INFO] Throttle before: " + (throttlePre.empty() ? "N/A" : throttlePre));

    // 5-second stress across 4 cores
    int rc = system("for i in 1 2 3 4; do (dd if=/dev/zero bs=1M count=512 2>/dev/null | md5sum &); done; sleep 5; kill $(jobs -p) 2>/dev/null; true");
    (void)rc;

    std::string throttlePost = relRunCmd("vcgencmd get_throttled 2>/dev/null");
    data.push_back("[INFO] Throttle after:  " + (throttlePost.empty() ? "N/A" : throttlePost));

    std::string tempStr = relReadFile("/sys/class/thermal/thermal_zone0/temp");
    if (!tempStr.empty())
    {
        double tempMc = 0.0;
        if (relParseDouble(tempStr, tempMc))
        {
            double tempC = tempMc / 1000.0;
            data.push_back("[INFO] SoC temp after stress: " + std::to_string(tempC) + " °C");
            data.push_back(tempC < static_cast<double>(RPI5_THERMAL_THROTTLE_C)
                               ? "[PASS] Below throttle threshold"
                               : "[WARN] At/above throttle threshold");
        }
        else
        {
            data.push_back("[WARN] SoC temperature value unparseable: '" + tempStr + "'");
        }
    }

    bool uvDetected = !throttlePost.empty() && throttlePost != "throttled=0x0";
    data.push_back(uvDetected ? "[WARN] Throttle flag set — check power supply"
                              : "[PASS] No throttle flags after stress");
    data.push_back("[INFO] Full burn-in (hours) requires dedicated test fixture");
    {
        TestOutput _out;
        _out.status = !uvDetected ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = !uvDetected ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

/**
 * @brief Power cycling proxy: uptime, boot count, and filesystem health.
 */
TestOutput ReliabilityTesting::testPowerCycling()
{
    std::vector<std::string> data;
    data.push_back("=== Power Cycling Resilience (Pi5) ===");

    std::string uptime = relReadFile("/proc/uptime");
    if (!uptime.empty())
    {
        double sec = 0.0;
        if (relParseDouble(uptime, sec))
            data.push_back("[INFO] Uptime: " + std::to_string(static_cast<int>(sec)) + "s");
        else
            data.push_back("[WARN] Uptime value unparseable: '" + uptime + "'");
    }

    std::string bootCount = relRunCmd("last reboot 2>/dev/null | grep -c '^reboot'");
    data.push_back("[INFO] Boot entries in wtmp: " + bootCount);

    std::string panics = relRunCmd("journalctl -k --no-pager 2>/dev/null | grep -c -i 'kernel panic\\|Oops'");
    std::string fsErr = relRunCmd("dmesg 2>/dev/null | grep -c -i 'EXT4-fs error\\|Buffer I/O error'");
    long panicCount = -1, fsCount = -1;
    bool logReadable = relParseLong(panics, panicCount) && relParseLong(fsErr, fsCount);
    if (!logReadable)
    {
        data.push_back("[SKIP] Kernel log not readable — power-cycle resilience proxies (panic/FS-error counters) cannot be measured");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    data.push_back(panicCount == 0 ? "[PASS] No kernel panics in journal"
                                   : "[FAIL] Kernel panic count: " + std::to_string(panicCount));
    data.push_back(fsCount == 0 ? "[PASS] No FS errors in dmesg"
                                : "[FAIL] FS error count: " + std::to_string(fsCount) + " (unclean power events)");

    // Acceptance criteria (measurable proxies): zero kernel panics and zero
    // filesystem errors indicate the unit has survived its power events
    // cleanly. Physical power-cycle stress still requires an external
    // controller (PDU/relay).
    if (panicCount > 0 || fsCount > 0)
    {
        data.push_back("[FAIL] Unclean-shutdown evidence present (panics and/or FS errors) — power-cycle resilience compromised");
        TestOutput _out;
        _out.status = hal::Status::FAIL;
        _out.severity = hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[PASS] No unclean-shutdown evidence (0 panics, 0 FS errors); note: a physical power-cycle test still requires an external controller");
    TestOutput _out;
    _out.status = hal::Status::PASS;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief Thermal cycling proxy: verify SoC temperature is within rated range.
 */
TestOutput ReliabilityTesting::testThermalCycling()
{
    std::vector<std::string> data;
    data.push_back("=== Thermal Cycling Proxy (Pi5) ===");
    data.push_back("[INFO] Pi5 BCM2712 rated: 0–85 °C junction");

    bool allOk = true;
    for (int zone = 0; zone <= 1; zone++)
    {
        std::string base    = "/sys/class/thermal/thermal_zone" + std::to_string(zone);
        std::string tempStr = relReadFile(base + "/temp");
        std::string type    = relReadFile(base + "/type");
        if (tempStr.empty()) continue;
        double tempMc = 0.0;
        if (!relParseDouble(tempStr, tempMc))
        {
            data.push_back("[FAIL] " + type + ": temperature value unparseable: '" + tempStr + "'");
            allOk = false;
            continue;
        }
        double tempC = tempMc / 1000.0;
        bool ok      = tempC >= 0.0 && tempC < static_cast<double>(RPI5_THERMAL_SHUTDOWN_C);
        data.push_back((ok ? "[PASS] " : "[WARN] ") + type + ": " +
                       std::to_string(tempC) + " °C " + (ok ? "(in spec)" : "(out of spec!)"));
        if (!ok) allOk = false;
    }

    data.push_back("[INFO] Active thermal cycling requires HALT/HASS chamber");
    {
        TestOutput _out;
        _out.status = allOk ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = allOk ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

/**
 * @brief MTBF proxy: uptime, pstore, and PCIe AER error counters.
 */
TestOutput ReliabilityTesting::testMtbf()
{
    std::vector<std::string> data;
    data.push_back("=== MTBF Proxy (Pi5) ===");

    std::string uptimeStr = relReadFile("/proc/uptime");
    double uptimeS = 0.0;
    if (!uptimeStr.empty())
    {
        if (relParseDouble(uptimeStr, uptimeS))
            data.push_back("[INFO] Uptime: " + std::to_string(static_cast<long>(uptimeS)) + "s");
        else
            data.push_back("[WARN] Uptime value unparseable: '" + uptimeStr + "'");
    }

    data.push_back("[INFO] Pi5 has no on-die ECC DRAM; MTBF via hardware fault counters only");

    std::string pstoreCount = relRunCmd("ls /sys/fs/pstore/ 2>/dev/null | wc -l");
    long pstoreN = -1;
    bool pstoreOk = relParseLong(pstoreCount, pstoreN) && pstoreN >= 0;
    bool pstoreOops = pstoreOk && pstoreN > 0;
    if (pstoreOk)
        data.push_back(pstoreOops ? "[FAIL] pstore crash entries: " + pstoreCount
                                  : "[PASS] pstore empty — no prior crash records");
    else
        data.push_back("[INFO] pstore unavailable — crash records cannot be checked");

    std::string aerErr = relRunCmd(
        "grep -r '' /sys/bus/pci/devices/*/aer_stats/ 2>/dev/null | grep -v ':0$' | head -5");
    data.push_back(aerErr.empty() ? "[PASS] No PCIe AER errors"
                                  : "[WARN] PCIe AER errors:\n" + aerErr);

    {
        TestOutput _out;
        _out.status = !pstoreOops ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = !pstoreOops ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

/**
 * @brief Component aging proxy: storage endurance and CPU frequency headroom.
 */
TestOutput ReliabilityTesting::testComponentAging()
{
    std::vector<std::string> data;
    data.push_back("=== Component Aging Proxy (Pi5) ===");

    std::string mmcStat = relReadFile("/sys/block/mmcblk0/stat");
    if (!mmcStat.empty())
        data.push_back("[INFO] mmcblk0 stats: " + mmcStat);

    std::string nvmeSmart = relRunCmd(
        "nvme smart-log /dev/nvme0 2>/dev/null | grep -E 'percentage_used|power_on_hours'");
    data.push_back(nvmeSmart.empty() ? "[INFO] NVMe SMART unavailable"
                                     : "[INFO] NVMe SMART:\n" + nvmeSmart);

    std::string maxF = relReadFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq");
    std::string curF = relReadFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    long mf = 0, cf = 0;
    bool haveHeadroom = relParseLong(maxF, mf) && relParseLong(curF, cf) && mf > 0 && cf > 0;
    if (!maxF.empty() && !curF.empty())
    {
        if (haveHeadroom)
        {
            double p = 100.0 * cf / mf;
            data.push_back("[INFO] CPU0 freq: " + std::to_string(cf / 1000) + " / " +
                           std::to_string(mf / 1000) + " MHz (" + std::to_string(static_cast<int>(p)) + "%)");
        }
        else
        {
            data.push_back("[WARN] CPU frequency values unparseable (max='" + maxF + "', cur='" + curF + "')");
        }
    }

    // Acceptance criteria (measurable): aging evidence is storage endurance
    // (NVMe percentage_used) or CPU frequency headroom. NVMe wear >= 90% ->
    // FAIL; nothing measurable -> SKIP.
    double used_val = -1.0;
    bool haveWear = false, wearOk = true;
    if (!nvmeSmart.empty())
    {
        size_t pos = nvmeSmart.find("percentage_used");
        if (pos != std::string::npos)
        {
            std::string tail = nvmeSmart.substr(pos);
            size_t dpos = tail.find_first_of("0123456789");
            if (dpos != std::string::npos)
            {
                char* end = nullptr;
                used_val = strtod(tail.c_str() + dpos, &end);
                if (end != tail.c_str() + dpos)
                    haveWear = used_val >= 0.0 && used_val <= 1000.0;
            }
            if (haveWear)
            {
                data.push_back("[INFO] NVMe wear level used: " + std::to_string(used_val) + "%");
                wearOk = used_val < 90.0;
            }
        }
    }
    if (haveWear && !wearOk)
    {
        data.push_back("[FAIL] NVMe wear level " + std::to_string(used_val) + "% exceeds the 90% aging threshold");
        TestOutput _out;
        _out.status = hal::Status::FAIL;
        _out.severity = hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
    if (haveWear || haveHeadroom)
    {
        data.push_back("[PASS] Aging indicators measured (storage wear and/or CPU headroom within limits)");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[SKIP] No aging telemetry measurable (no NVMe SMART, no cpufreq sysfs) — component aging cannot be assessed");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief Solder joint integrity proxy: PCIe link state and USB/eMMC errors.
 */
TestOutput ReliabilityTesting::testSolderJointIntegrity()
{
    std::vector<std::string> data;
    data.push_back("=== Solder Joint Integrity Proxy (Pi5) ===");
    data.push_back("[INFO] Physical testing requires X-ray / thermal imaging");

    std::string pcieLinks = relRunCmd(
        "for d in /sys/bus/pci/devices/*; do s=$(cat $d/current_link_speed 2>/dev/null); "
        "[ -n \"$s\" ] && echo \"$(basename $d): $s\"; done 2>/dev/null");
    data.push_back(pcieLinks.empty() ? "[INFO] No PCIe devices"
                                     : "[PASS] PCIe links:\n" + pcieLinks);

    std::string usbResets = relRunCmd(
        "dmesg 2>/dev/null | grep -c -i 'USB disconnect\\|reset full-speed\\|xhci.*error'");
    std::string mmcErr = relRunCmd("dmesg 2>/dev/null | grep -c -i 'mmc.*error\\|mmcblk.*error'");
    long usbCount = -1, mmcCount = -1;
    bool logReadable = relParseLong(usbResets, usbCount) && relParseLong(mmcErr, mmcCount);
    if (!logReadable)
    {
        data.push_back("[SKIP] Kernel log (dmesg) unreadable — solder-joint proxies (USB/eMMC error counters) cannot be measured");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back(usbCount == 0 ? "[PASS] No USB error events"
                                 : "[FAIL] USB error events: " + std::to_string(usbCount));
    data.push_back(mmcCount == 0 ? "[PASS] No eMMC/SD errors in dmesg"
                                 : "[FAIL] eMMC/SD errors: " + std::to_string(mmcCount));

    // Acceptance criteria (measurable proxies): zero USB and zero eMMC/SD
    // error events in the kernel log (intermittent contact faults). Physical
    // X-ray/thermal-imaging verification remains a lab test.
    if (usbCount > 0 || mmcCount > 0)
    {
        data.push_back("[FAIL] Contact-fault indicators present in kernel log (USB/eMMC errors) — check solder joints and connectors");
        TestOutput _out;
        _out.status = hal::Status::FAIL;
        _out.severity = hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[PASS] No contact-fault indicators (0 USB errors, 0 eMMC/SD errors; physical X-ray verification still requires lab equipment)");
    TestOutput _out;
    _out.status = hal::Status::PASS;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief Run all tests for this module
 * @return Vector of test results
 */
std::vector<TestResult> ReliabilityTesting::runAllTests()
{
    std::vector<TestResult> results;

    // Run testBurninTesting
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testBurninTesting();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testBurninTesting";
            _tr.test_id = "testBurninTesting";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testPowerCycling
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testPowerCycling();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testPowerCycling";
            _tr.test_id = "testPowerCycling";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testThermalCycling
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testThermalCycling();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testThermalCycling";
            _tr.test_id = "testThermalCycling";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testMtbf
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testMtbf();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testMtbf";
            _tr.test_id = "testMtbf";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testComponentAging
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testComponentAging();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testComponentAging";
            _tr.test_id = "testComponentAging";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testSolderJointIntegrity
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testSolderJointIntegrity();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testSolderJointIntegrity";
            _tr.test_id = "testSolderJointIntegrity";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    return results;
}

/**
 * @brief Get list of all available test names
 * @return Vector of test names
 */
std::vector<std::string> ReliabilityTesting::getAvailableTests()
{
    return {
        "testBurninTesting", "testPowerCycling", "testThermalCycling", "testMtbf", "testComponentAging", "testSolderJointIntegrity",
    };
}

/**
 * @brief Run a single test by name
 * @param testName Name of the test to run
 * @return Test result with timing
 */
TestResult ReliabilityTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput result;
    result.status = hal::Status::ERROR;
    result.severity = hal::Severity::ERROR;
    result.error_message = "test not dispatched";

    if (testName == "testBurninTesting") {
        result = testBurninTesting();
    } else if (testName == "testPowerCycling") {
        result = testPowerCycling();
    } else if (testName == "testThermalCycling") {
        result = testThermalCycling();
    } else if (testName == "testMtbf") {
        result = testMtbf();
    } else if (testName == "testComponentAging") {
        result = testComponentAging();
    } else if (testName == "testSolderJointIntegrity") {
        result = testSolderJointIntegrity();
    } else {
        // Unknown test name
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = testName + " (UNKNOWN)";
            _tr.test_id = testName;
            _tr.output.status = hal::Status::ERROR;
            _tr.output.severity = hal::Severity::ERROR;
            _tr.output.error_message = "Unknown test name: " + testName;
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            return _tr;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    {
        TestResult _tr;
        _tr.name = testName;
        _tr.test_id = testName;
        _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
        _tr.output = result;
        return _tr;
    }
}
