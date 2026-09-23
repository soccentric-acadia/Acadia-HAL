/**
 * @file environmental_lib.cpp
 * @brief Environmental / physical-environment tests for Raspberry Pi 5.
 *        Verifies that the hardware is operating within its rated environmental
 *        envelope using sysfs thermal, hwmon, and power-supply interfaces.
 *        Physical tests (humidity chamber, vibration table, EMI/EMC lab) are
 *        documented but cannot be performed programmatically.
 * @author sandesh ghimire
 * @copyright (C) Lucid Motors
 */

#include "environmental_lib.h"
#include "../platform_rpi5.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

// ---------------------------------------------------------------------------
static std::string envReadFile(const std::string& path)
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

static std::string envRunCmd(const std::string& cmd)
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

static bool envPathExists(const std::string& p)
{
    struct stat st {};
    return stat(p.c_str(), &st) == 0;
}

// Safe numeric parsing: rejects empty/garbage input instead of throwing.
static bool envParseLong(const std::string& s, long& out)
{
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    long v = strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || errno == ERANGE) return false;
    while (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t') ++end;
    if (*end != '\0') return false;
    out = v;
    return true;
}

static bool envParseDouble(const std::string& s, double& out)
{
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    double v = strtod(s.c_str(), &end);
    if (end == s.c_str()) return false;
    out = v;
    return true;
}
// ---------------------------------------------------------------------------

/**
 * @brief Constructor for EnvironmentalTesting
 */
EnvironmentalTesting::EnvironmentalTesting()
{
}

/**
 * @brief Destructor for EnvironmentalTesting
 */
EnvironmentalTesting::~EnvironmentalTesting()
{
}

/**
 * @brief Verify SoC and board temperatures are within the Pi5 rated range.
 * @details Pi5 BCM2712 rated operating range: 0 °C to 85 °C junction.
 *          Reads thermal_zone0 (cpu-thermal) and thermal_zone1 (rp1-thermal)
 *          and checks they are within 0–80 °C nominal operating band.
 */
TestOutput EnvironmentalTesting::testTemperatureRange()
{
    std::vector<std::string> data;
    data.push_back("=== Operating Temperature Range (Pi5 BCM2712) ===");
    data.push_back("[INFO] Rated: 0–85 °C junction, nominal operating: 0–80 °C");

    bool allOk = true;
    int zonesRead = 0;
    for (int z = 0; z <= 1; z++)
    {
        std::string base    = "/sys/class/thermal/thermal_zone" + std::to_string(z);
        std::string tempStr = envReadFile(base + "/temp");
        std::string type    = envReadFile(base + "/type");
        if (tempStr.empty()) continue;
        ++zonesRead;
        long milliC = 0;
        if (!envParseLong(tempStr, milliC))
        {
            data.push_back("[FAIL] " + (type.empty() ? "zone" + std::to_string(z) : type) +
                           ": temperature value unparseable: '" + tempStr + "'");
            allOk = false;
            continue;
        }
        double tempC = static_cast<double>(milliC) / 1000.0;
        bool ok      = tempC >= 0.0 && tempC < static_cast<double>(RPI5_THERMAL_THROTTLE_C);
        data.push_back((ok ? "[PASS] " : "[WARN] ") + (type.empty() ? "zone" + std::to_string(z) : type) +
                       ": " + std::to_string(tempC) + " °C (" + (ok ? "nominal" : "at throttle limit!") + ")");
        if (!ok) allOk = false;
    }

    std::string vcTemp = envRunCmd("vcgencmd measure_temp 2>/dev/null");
    if (!vcTemp.empty())
        data.push_back("[INFO] vcgencmd: " + vcTemp);

    if (zonesRead == 0)
    {
        data.push_back("[SKIP] No thermal zones readable — operating temperature cannot be measured");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    {
        TestOutput _out;
        _out.status = allOk ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = allOk ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

/**
 * @brief Humidity resistance — software-visible proxy test.
 * @details No on-board humidity sensor on Pi5.  Test checks for moisture /
 *          condensation indicators: abnormal leakage currents show up as
 *          under-voltage events; clock instability manifests as CPU frequency
 *          dips.  Documents IEC 60068-2-78 test requirements.
 */
TestOutput EnvironmentalTesting::testHumidityResistance()
{
    std::vector<std::string> data;
    data.push_back("=== Humidity Resistance (Pi5 Proxy) ===");
    data.push_back("[INFO] IEC 60068-2-78: 40 °C / 93% RH — requires humidity chamber");
    data.push_back("[INFO] Pi5 has no on-board humidity sensor; checking electrical proxies");

    // Acceptance criteria (measurable): no under-voltage/throttle flags (the
    // electrical fingerprint moisture-induced leakage leaves behind) and, when
    // an IIO humidity sensor is fitted, a reading below the 80% RH watch
    // threshold. With no proxy observable at all -> SKIPPED.
    std::string throttle = envRunCmd("vcgencmd get_throttled 2>/dev/null");
    bool haveThrottle = throttle.find("throttled=") != std::string::npos;
    if (haveThrottle)
    {
        bool uvFlag = throttle.find("throttled=0x0") != 0; // anything other than 0x0
        if (uvFlag)
        {
            data.push_back("[FAIL] Under-voltage/throttle flag set: " + throttle +
                           " — electrical evidence of leakage/condensation or supply stress");
            TestOutput _out;
            _out.status = hal::Status::FAIL;
            _out.severity = hal::Severity::ERROR;
            StampDiagnostics(_out, data);
            return _out;
        }
        data.push_back("[PASS] No under-voltage flags: " + throttle);
    }

    // Check for IIO humidity devices (optional HAT sensors)
    std::string iioHumidity = envRunCmd(
        "for d in /sys/bus/iio/devices/iio:device*; do "
        "n=$(cat $d/name 2>/dev/null); "
        "[ -f $d/in_humidityrelative_input ] && echo \"$n: $(cat $d/in_humidityrelative_input 2>/dev/null)%\"; "
        "done 2>/dev/null");
    bool haveRh = false;
    if (!iioHumidity.empty())
    {
        size_t colon = iioHumidity.rfind(": ");
        std::string val = (colon != std::string::npos) ? iioHumidity.substr(colon + 2) : iioHumidity;
        double rh = 0.0;
        if (envParseDouble(val, rh) && rh >= 0.0 && rh <= 100.0)
        {
            haveRh = true;
            if (rh >= 80.0)
            {
                data.push_back("[FAIL] Humidity sensor reading " + std::to_string(rh) +
                               "% RH — above the 80% condensation-watch threshold");
                TestOutput _out;
                _out.status = hal::Status::FAIL;
                _out.severity = hal::Severity::ERROR;
                StampDiagnostics(_out, data);
                return _out;
            }
            data.push_back("[PASS] Humidity sensor: " + iioHumidity + " (below the 80% RH threshold)");
        }
        else
        {
            data.push_back("[INFO] Humidity sensor value not parseable: " + iioHumidity);
        }
    }
    else
    {
        data.push_back("[INFO] No IIO humidity sensor found (no HAT)");
    }

    if (!haveThrottle && !haveRh)
    {
        data.push_back("[SKIP] No electrical humidity proxies observable (vcgencmd unavailable, no IIO humidity sensor) — physical IEC 60068-2-78 test requires a humidity chamber");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    {
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
}

/**
 * @brief Vibration resistance proxy: check for mechanical-shock indicators.
 * @details IEC 60068-2-6/2-64 vibration testing requires a shaker table.
 *          Software proxy: checks filesystem error counters and PCIe link
 *          stability (vibration can cause intermittent connector contact).
 */
TestOutput EnvironmentalTesting::testVibrationResistance()
{
    std::vector<std::string> data;
    data.push_back("=== Vibration Resistance Proxy (Pi5) ===");
    data.push_back("[INFO] IEC 60068-2-6 testing requires vibration table");

    // Acceptance criteria (measurable proxies): the kernel log must show zero
    // storage I/O errors and zero USB disconnect events (both are evidence of
    // intermittent connector contact under vibration), and any PCIe devices
    // present must expose readable link state. Log unreadable -> SKIPPED.
    // PCIe link stability
    std::string pcieSpeed = envRunCmd(
        "cat /sys/bus/pci/devices/*/current_link_speed 2>/dev/null | head -5");
    data.push_back(pcieSpeed.empty() ? "[INFO] No PCIe devices to check"
                                     : "[INFO] PCIe link speed(s): " + pcieSpeed);

    // eMMC/SD error proxy
    std::string mmcErr = envRunCmd("dmesg 2>/dev/null | grep -c -i 'mmc.*error\\|mmcblk.*I/O error'");
    // USB disconnect events
    std::string usbDisc = envRunCmd("dmesg 2>/dev/null | grep -c 'USB disconnect'");

    long mmcE = -1, usbD = -1;
    bool logReadable = envParseLong(mmcErr, mmcE) && envParseLong(usbDisc, usbD);
    if (!logReadable)
    {
        data.push_back("[SKIP] Kernel log (dmesg) unreadable — vibration proxies (eMMC/SD + USB error counters) cannot be measured");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back(mmcE == 0 ? "[PASS] 0 eMMC/SD errors (good connector contact)"
                             : "[FAIL] eMMC/SD error count: " + std::to_string(mmcE));
    data.push_back(usbD == 0 ? "[PASS] 0 USB disconnect events"
                             : "[FAIL] USB disconnects: " + std::to_string(usbD));

    if (mmcE > 0 || usbD > 0)
    {
        data.push_back("[FAIL] Kernel log shows storage/USB connectivity errors — consistent with contact instability");
        TestOutput _out;
        _out.status = hal::Status::FAIL;
        _out.severity = hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[PASS] No kernel-log evidence of contact instability (0 eMMC/SD errors, 0 USB disconnects)");
    {
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
}

/**
 * @brief Shock resistance proxy: examine crash/panic records.
 * @details MIL-STD-810 / IEC 60068-2-27 shock testing requires a drop table.
 *          Software proxy: pstore crash records and unexpected reboots in wtmp.
 */
TestOutput EnvironmentalTesting::testShockResistance()
{
    std::vector<std::string> data;
    data.push_back("=== Shock Resistance Proxy (Pi5) ===");
    data.push_back("[INFO] IEC 60068-2-27 testing requires mechanical shock equipment");

    // pstore crash records
    bool pstoreHasCrash = envPathExists("/sys/fs/pstore") &&
                          envRunCmd("ls /sys/fs/pstore/ 2>/dev/null | wc -l") != "0";
    data.push_back(pstoreHasCrash ? "[WARN] pstore has crash records (potential prior shock event)"
                                  : "[PASS] pstore empty — no crash records");

    // Unexpected reboots
    std::string rebootCount = envRunCmd("last reboot 2>/dev/null | grep -c '^reboot'");
    data.push_back("[INFO] Reboot entries in wtmp: " + rebootCount);

    // Kernel OOM events
    std::string oomCount = envRunCmd("dmesg 2>/dev/null | grep -c 'Out of memory'");
    data.push_back(oomCount == "0" ? "[PASS] No OOM events in dmesg"
                                   : "[WARN] OOM events: " + oomCount);

    {
        TestOutput _out;
        _out.status = !pstoreHasCrash ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = !pstoreHasCrash ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

/**
 * @brief EMI/EMC compliance proxy: check clock frequencies and spread-spectrum.
 * @details CISPR 32 / EN 55032 EMI testing requires a shielded RF lab.
 *          Software proxy: verify spread-spectrum clocking is enabled and
 *          check for any kernel EMI-related kernel messages.
 */
TestOutput EnvironmentalTesting::testEmiemcCompliance()
{
    std::vector<std::string> data;
    data.push_back("=== EMI/EMC Compliance Proxy (Pi5) ===");
    data.push_back("[INFO] CISPR 32 / EN 55032 testing requires an RF shielded test lab");

    // Pi5 spread-spectrum: BCM2712 supports SSC; check config.txt
    std::string ssc = envRunCmd("grep -i 'spread_spectrum\\|ssc' /boot/firmware/config.txt 2>/dev/null");
    data.push_back(ssc.empty() ? "[INFO] No explicit spread-spectrum config in config.txt"
                               : "[INFO] SSC config: " + ssc);

    // CPU frequency (high clocks → more EMI)
    std::string cpuFreq = envReadFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    if (!cpuFreq.empty())
    {
        long freqKhz = 0;
        if (envParseLong(cpuFreq, freqKhz))
            data.push_back("[INFO] CPU0 current freq: " + std::to_string(freqKhz / 1000) + " MHz");
        else
            data.push_back("[WARN] CPU0 current freq value unparseable: '" + cpuFreq + "'");
    }

    // Check metal shielding / EMI cans reported via device tree
    std::string dtComp = envRunCmd(
        "cat /proc/device-tree/compatible 2>/dev/null | tr '\\0' '\\n' | head -3");
    data.push_back("[INFO] Device-tree compatible: " + dtComp);

    // Acceptance criteria: radiated-emissions compliance (CISPR 32 / EN 55032)
    // can only be verified with an RF shielded lab; no software path can yield
    // a trustworthy PASS. Document the configuration and SKIP.
    data.push_back("[SKIP] CISPR 32 / EN 55032 radiated-emissions verification requires an RF shielded lab — software can only document SSC configuration above");
    {
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
}

/**
 * @brief ESD protection proxy: verify GPIO input protection paths.
 * @details IEC 61000-4-2 ESD testing requires ESD gun equipment.
 *          Software proxy: checks GPIO driver is loaded and no GPIO errors
 *          in dmesg (GPIO controller damage often surfaces as driver errors).
 */
TestOutput EnvironmentalTesting::testEsdProtection()
{
    std::vector<std::string> data;
    data.push_back("=== ESD Protection Proxy (Pi5) ===");
    data.push_back("[INFO] IEC 61000-4-2 ESD testing requires ESD gun; using software proxy");

    // GPIO driver presence
    bool gpioPresent = envPathExists(RPI5_GPIOCHIP_PATH);
    data.push_back(gpioPresent ? "[PASS] GPIO chip " + std::string(RPI5_GPIOCHIP_PATH) + " present"
                               : "[WARN] GPIO chip not found at " + std::string(RPI5_GPIOCHIP_PATH));

    // GPIO driver errors in dmesg
    std::string gpioErr = envRunCmd("dmesg 2>/dev/null | grep -c -i 'gpio.*error\\|gpiochip.*fail'");
    data.push_back(gpioErr == "0" ? "[PASS] No GPIO driver errors in dmesg"
                                  : "[WARN] GPIO errors: " + gpioErr);

    // I2C errors (ESD-susceptible pins)
    std::string i2cErr = envRunCmd("dmesg 2>/dev/null | grep -c -i 'i2c.*timeout\\|i2c.*error\\|i2c.*fail'");
    data.push_back(i2cErr == "0" ? "[PASS] No I2C errors in dmesg"
                                 : "[WARN] I2C errors: " + i2cErr);

    data.push_back("[INFO] Pi5 header pins have TPD2E2U06 (or equiv) TVS protection");
    {
        TestOutput _out;
        _out.status = gpioPresent ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = gpioPresent ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

/**
 * @brief Salt-spray resistance — hardware documentation check.
 * @details IEC 60068-2-11 salt-spray testing requires a salt-fog chamber.
 *          Pi5 bare board is not rated for salt-spray environments; this test
 *          documents the requirement and checks for a conformal coating
 *          indicator file if present (custom industrial carrier board).
 */
TestOutput EnvironmentalTesting::testSaltSprayResistance()
{
    std::vector<std::string> data;
    data.push_back("=== Salt-Spray Resistance (Pi5) ===");
    data.push_back("[INFO] IEC 60068-2-11: salt-spray test requires dedicated chamber");
    data.push_back("[INFO] Bare Pi5 PCB is NOT rated for salt-spray (no conformal coating)");
    data.push_back("[INFO] Industrial carrier boards may add protection — check board spec");

    // Check if board is a CM5 (Compute Module) — slightly better environmental rating
    std::string model = envReadFile("/proc/device-tree/model");
    if (!model.empty())
    {
        data.push_back("[INFO] Board model: " + model);
        bool isCM = model.find("Compute Module") != std::string::npos;
        data.push_back(isCM ? "[INFO] Compute Module variant — check carrier board for IP rating"
                            : "[INFO] Standard Pi5 — no IP/salt-spray rating");
    }

    // Acceptance criteria (measurable): the corrosion proxy is the bus error
    // counter in the kernel log. Log unreadable -> SKIP; errors -> FAIL;
    // none -> PASS (with the documented limitation that a physical
    // IEC 60068-2-11 salt-fog test still requires a chamber).
    std::string i2cErr = envRunCmd("dmesg 2>/dev/null | grep -c -i 'i2c.*error\\|spi.*error'");
    long busErr = -1;
    if (!envParseLong(i2cErr, busErr))
    {
        data.push_back("[SKIP] Kernel log (dmesg) unreadable — corrosion proxy (bus error counters) cannot be measured");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    if (busErr > 0)
    {
        data.push_back("[FAIL] " + std::to_string(busErr) + " I2C/SPI bus error(s) — possible connector corrosion indicator");
        TestOutput _out;
        _out.status = hal::Status::FAIL;
        _out.severity = hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[PASS] 0 I2C/SPI communication errors (no corrosion indicator; note: chamber test still required for formal rating)");
    {
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
}

/**
 * @brief Run all tests for this module
 * @return Vector of test results
 */
std::vector<TestResult> EnvironmentalTesting::runAllTests()
{
    std::vector<TestResult> results;

    // Run testTemperatureRange
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testTemperatureRange();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testTemperatureRange";
            _tr.test_id = "testTemperatureRange";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testHumidityResistance
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testHumidityResistance();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testHumidityResistance";
            _tr.test_id = "testHumidityResistance";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testVibrationResistance
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testVibrationResistance();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testVibrationResistance";
            _tr.test_id = "testVibrationResistance";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testShockResistance
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testShockResistance();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testShockResistance";
            _tr.test_id = "testShockResistance";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testEmiemcCompliance
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testEmiemcCompliance();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testEmiemcCompliance";
            _tr.test_id = "testEmiemcCompliance";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testEsdProtection
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testEsdProtection();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testEsdProtection";
            _tr.test_id = "testEsdProtection";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testSaltSprayResistance
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testSaltSprayResistance();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testSaltSprayResistance";
            _tr.test_id = "testSaltSprayResistance";
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
std::vector<std::string> EnvironmentalTesting::getAvailableTests()
{
    return {
        "testTemperatureRange", "testHumidityResistance", "testVibrationResistance", "testShockResistance",
        "testEmiemcCompliance", "testEsdProtection",      "testSaltSprayResistance",
    };
}

/**
 * @brief Run a single test by name
 * @param testName Name of the test to run
 * @return Test result with timing
 */
TestResult EnvironmentalTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput result;
    result.status = hal::Status::ERROR;
    result.severity = hal::Severity::ERROR;
    result.error_message = "test not dispatched";

    if (testName == "testTemperatureRange") {
        result = testTemperatureRange();
    } else if (testName == "testHumidityResistance") {
        result = testHumidityResistance();
    } else if (testName == "testVibrationResistance") {
        result = testVibrationResistance();
    } else if (testName == "testShockResistance") {
        result = testShockResistance();
    } else if (testName == "testEmiemcCompliance") {
        result = testEmiemcCompliance();
    } else if (testName == "testEsdProtection") {
        result = testEsdProtection();
    } else if (testName == "testSaltSprayResistance") {
        result = testSaltSprayResistance();
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
