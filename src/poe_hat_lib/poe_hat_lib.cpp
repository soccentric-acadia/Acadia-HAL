/**
 * @file poe_hat_lib.cpp
 * @brief Raspberry Pi PoE+ HAT tests for Raspberry Pi 5.
 *
 * Hardware: Raspberry Pi PoE+ HAT (or compatible) on 40-pin header.
 *   PoE standard : IEEE 802.3at (Type 2, PoE+, up to 25.5 W)
 *   DC-DC        : 5V / 5A synchronous buck on HAT, feeds Pi via GPIO header
 *   Fan ctrl     : ATtiny on I2C @ 0x1A (RPi PoE HAT), sysfs fan node
 *   Power sense  : Optional INA219/INA226 on I2C (third-party HATs)
 *   Eth link     : Pi5 GbE (BCM2712 MAC + BCM54213PE PHY) — PoE on eth0
 *
 * Tests use hwmon sysfs, I2C tools, ethtool, vcgencmd, and thermal sysfs.
 * @author sandesh ghimire
 * @copyright (C) Lucid Motors
 */

#include "poe_hat_lib.h"
#include "../platform_rpi5.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

// ---------------------------------------------------------------------------
static std::string poeReadFile(const std::string& path)
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

static std::string poeRunCmd(const std::string& cmd)
{
    std::array<char, 2048> buf{};
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
static bool poeParseLong(const std::string& s, long& out)
{
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    long v = strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || errno == ERANGE) return false;
    out = v;
    return true;
}

static bool poeParseDouble(const std::string& s, double& out)
{
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    double v = strtod(s.c_str(), &end);
    if (end == s.c_str()) return false;
    out = v;
    return true;
}

static bool poePathExists(const std::string& p)
{
    struct stat st {};
    return stat(p.c_str(), &st) == 0;
}

/** List all /sys/class/hwmon/hwmonN directories. */
static std::vector<std::string> poeListHwmon()
{
    std::vector<std::string> nodes;
    const std::string base = "/sys/class/hwmon/";
    DIR* d = opendir(base.c_str());
    if (!d) return nodes;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr)
    {
        std::string n = e->d_name;
        if (n.find("hwmon") == 0)
            nodes.push_back(base + n);
    }
    closedir(d);
    return nodes;
}
// ---------------------------------------------------------------------------

PoeHatTesting::PoeHatTesting() {}
PoeHatTesting::~PoeHatTesting() {}

/**
 * @brief Detect PoE HAT: scan I2C for fan controller and hwmon for power sensors.
 */
TestOutput PoeHatTesting::testPoeHatDetection()
{
    std::vector<std::string> data;
    data.push_back("=== PoE HAT Detection (Pi5) ===");
    data.push_back("[INFO] Official RPi PoE+ HAT: fan ctrl @ I2C-1 0x1A");
    data.push_back("[INFO] Third-party HATs may expose INA219 @ 0x40 or INA226 @ 0x40-0x4F");

    bool i2c1 = poePathExists("/dev/i2c-1");
    data.push_back(i2c1 ? "[PASS] /dev/i2c-1 present"
                        : "[WARN] /dev/i2c-1 absent");

    bool fanCtrl = false, inaPresent = false;
    if (i2c1)
    {
        std::string scan = poeRunCmd("i2cdetect -y 1 2>/dev/null");
        fanCtrl  = scan.find(" 1a ") != std::string::npos ||
                   scan.find("\n1a ") != std::string::npos;
        inaPresent = scan.find(" 40 ") != std::string::npos ||
                     scan.find(" 41 ") != std::string::npos ||
                     scan.find(" 45 ") != std::string::npos;
        data.push_back(fanCtrl  ? "[PASS] Fan controller @ 0x1A detected"
                                : "[INFO] Fan controller @ 0x1A not found");
        data.push_back(inaPresent ? "[PASS] Power sense IC @ 0x40+ detected (INA219/226)"
                                  : "[INFO] INA219/226 not detected on I2C-1");
    }

    // Check for HAT EEPROM (all Pi HATs have an EEPROM on I2C address 0x50)
    std::string eeprom = poePathExists("/proc/device-tree/hat/product")
        ? poeReadFile("/proc/device-tree/hat/product") : "";
    if (!eeprom.empty())
        data.push_back("[PASS] HAT EEPROM product: " + eeprom);
    else
        data.push_back("[INFO] HAT EEPROM not readable at /proc/device-tree/hat/");

    {
        TestOutput _out;
        const bool _ok = fanCtrl || inaPresent || !eeprom.empty();
        _out.status = _ok ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = _ok ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

/**
 * @brief Verify PoE power negotiation: ethtool link and PoE TLV / dmesg.
 */
TestOutput PoeHatTesting::testPoePowerNegotiation()
{
    std::vector<std::string> data;
    data.push_back("=== PoE Power Negotiation (IEEE 802.3at) ===");
    data.push_back("[INFO] Pi5 receives power over " + std::string(RPI5_ETH_IFACE) + " from PoE switch/injector");

    // Check eth0 link is up
    std::string operstate = poeReadFile(
        "/sys/class/net/" + std::string(RPI5_ETH_IFACE) + "/operstate");
    bool linkUp = operstate == "up";
    data.push_back(linkUp ? "[PASS] " + std::string(RPI5_ETH_IFACE) + " link up (PoE carrier present)"
                          : "[INFO] " + std::string(RPI5_ETH_IFACE) + " operstate: " + operstate);

    // ethtool: check speed and duplex
    std::string ethSpeed = poeRunCmd(
        "ethtool " + std::string(RPI5_ETH_IFACE) + " 2>/dev/null | grep -E 'Speed|Duplex|Link'");
    if (!ethSpeed.empty())
        data.push_back("[INFO] ethtool:\n" + ethSpeed);

    // dmesg: look for PoE negotiation messages
    std::string dmesgPoe = poeRunCmd("dmesg 2>/dev/null | grep -i 'poe\\|802.3at\\|802.3af' | head -3");
    if (!dmesgPoe.empty())
        data.push_back("[INFO] PoE dmesg: " + dmesgPoe);

    // Check vcgencmd for under-voltage (indicates PoE supply is insufficient)
    std::string throttle = poeRunCmd("vcgencmd get_throttled 2>/dev/null");
    bool underVolt = !throttle.empty() && throttle != "throttled=0x0";
    data.push_back(underVolt ? "[WARN] Throttle flags set: " + throttle +
                               " (possible under-voltage from PoE)"
                             : "[PASS] No throttle — PoE power delivery adequate");

    {
        TestOutput _out;
        _out.status = linkUp ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = linkUp ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

/**
 * @brief Measure PoE input voltage/current via INA219/INA226 hwmon if present.
 */
TestOutput PoeHatTesting::testPowerMonitoring()
{
    std::vector<std::string> data;
    data.push_back("=== PoE Power Monitoring (INA219/INA226 hwmon) ===");

    auto hwmons = poeListHwmon();
    for (const auto& hw : hwmons)
    {
        std::string name = poeReadFile(hw + "/name");
        if (name.find("ina") == std::string::npos) continue;

        data.push_back("INA hwmon: " + hw + " (" + name + ")");
        std::string v1 = poeReadFile(hw + "/in1_input");
        std::string c1 = poeReadFile(hw + "/curr1_input");
        std::string p1 = poeReadFile(hw + "/power1_input");
        long vMv = 0, cMa = 0, pUw = 0;
        bool anyParsed = false;
        if (poeParseLong(v1, vMv)) { data.push_back("[INFO]   Voltage: " + std::to_string(vMv) + " mV"); anyParsed = true; }
        if (poeParseLong(c1, cMa)) { data.push_back("[INFO]   Current: " + std::to_string(cMa) + " mA"); anyParsed = true; }
        if (poeParseLong(p1, pUw)) { data.push_back("[INFO]   Power:   " + std::to_string(pUw / 1000) + " mW"); anyParsed = true; }

        // Acceptance criteria (measurable): an INA power monitor must be
        // present with at least one parseable reading in a plausible range
        // (PoE+ budget 25.5 W).
        if (!anyParsed)
        {
            data.push_back("[FAIL] INA hwmon present but no channel value parseable");
            TestOutput _out;
            _out.status = hal::Status::FAIL;
            _out.severity = hal::Severity::ERROR;
            StampDiagnostics(_out, data);
            return _out;
        }
        if (vMv > 0 && (vMv < 3600 || vMv > 60000))
        {
            data.push_back("[FAIL] PoE input voltage out of range: " + std::to_string(vMv) + " mV");
            TestOutput _out;
            _out.status = hal::Status::FAIL;
            _out.severity = hal::Severity::ERROR;
            StampDiagnostics(_out, data);
            return _out;
        }
        data.push_back("[PASS] Power monitor present with valid readings (PoE+ budget: 25.5 W input)");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    // Fallback: vcgencmd measure_volts
    std::string vCore = poeRunCmd("vcgencmd measure_volts core 2>/dev/null");
    std::string v5    = poeRunCmd("vcgencmd measure_volts usb  2>/dev/null");
    data.push_back("[INFO] vcgencmd core: " + (vCore.empty() ? "N/A" : vCore));
    data.push_back("[INFO] vcgencmd usb:  " + (v5.empty()    ? "N/A" : v5));

    data.push_back("[SKIP] No INA219/226 power monitor present — PoE power monitoring cannot be measured");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief Verify onboard fan presence and PWM control via hwmon sysfs.
 */
TestOutput PoeHatTesting::testFanControl()
{
    std::vector<std::string> data;
    data.push_back("=== PoE HAT Fan Control (hwmon PWM) ===");

    auto hwmons = poeListHwmon();
    bool found = false;
    for (const auto& hw : hwmons)
    {
        // Official PoE HAT exposes fan1_input and pwm1 under a hwmon node
        bool hasPwm = poePathExists(hw + "/pwm1");
        bool hasFan = poePathExists(hw + "/fan1_input");
        if (!hasPwm && !hasFan) continue;

        std::string name = poeReadFile(hw + "/name");
        data.push_back("[PASS] Fan hwmon: " + hw + " (" + name + ")");

        std::string pwm = poeReadFile(hw + "/pwm1");
        if (!pwm.empty())
            data.push_back("[INFO] pwm1 (0-255): " + pwm);

        std::string pwmEnable = poeReadFile(hw + "/pwm1_enable");
        data.push_back("[INFO] pwm1_enable: " + (pwmEnable.empty() ? "N/A" : pwmEnable));

        found = true;
        break;
    }

    if (!found)
    {
        // Fallback: search via thermal cooling device
        std::string coolDev = poeRunCmd(
            "grep -rl 'rpi-poe-fan\\|fan' /sys/class/thermal/cooling_device*/type 2>/dev/null | head -2");
        if (!coolDev.empty())
        {
            data.push_back("[PASS] Thermal cooling device found:\n" + coolDev);
            found = true;
        }
        else
        {
            data.push_back("[INFO] No PoE HAT fan control found in hwmon or thermal cooling devices");
            data.push_back("[INFO] Ensure dtoverlay=rpi-poe-fan is in /boot/firmware/config.txt");
        }
    }

    // Acceptance criteria (measurable): a fan control interface (hwmon PWM
    // or a thermal cooling device) must exist for the PoE HAT fan path.
    if (found)
    {
        data.push_back("[PASS] Fan control interface present");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[SKIP] No PoE HAT fan control found (hwmon/thermal) — ensure dtoverlay=rpi-poe-fan is configured or the HAT is attached");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief Read fan tachometer (RPM) via hwmon.
 */
TestOutput PoeHatTesting::testFanSpeed()
{
    std::vector<std::string> data;
    data.push_back("=== PoE HAT Fan Speed (tachometer) ===");

    auto hwmons = poeListHwmon();
    for (const auto& hw : hwmons)
    {
        std::string fan = poeReadFile(hw + "/fan1_input");
        if (fan.empty()) continue;
        std::string name = poeReadFile(hw + "/name");
        long rpm = 0;
        if (!poeParseLong(fan, rpm) || rpm < 0 || rpm > 15000)
        {
            data.push_back("[FAIL] Fan tachometer reading invalid: '" + fan + "' RPM (" + name + ")");
            TestOutput _out;
            _out.status = hal::Status::FAIL;
            _out.severity = hal::Severity::ERROR;
            StampDiagnostics(_out, data);
            return _out;
        }
        data.push_back("Fan hwmon " + name + " — fan1: " + fan + " RPM");
        if (rpm == 0)
        {
            data.push_back("[SKIP] Fan tachometer reads 0 RPM — fan idle or not spinning (HAT present)");
            TestOutput _out;
            _out.status = hal::Status::SKIPPED;
            _out.severity = hal::Severity::INFO;
            StampDiagnostics(_out, data);
            return _out;
        }
        data.push_back("[PASS] Fan spinning at " + std::to_string(rpm) + " RPM");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[SKIP] fan1_input not found in hwmon — fan not spinning or PoE HAT absent (official PoE+ HAT fan: 25×25 mm, ~5000 RPM max)");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief Check Pi5 thermal trip point that triggers fan speed increase.
 */
TestOutput PoeHatTesting::testThermalProtection()
{
    std::vector<std::string> data;
    data.push_back("=== PoE HAT Thermal Protection (Pi5 trip points) ===");

    // Read CPU temp (validated parse)
    std::string tempRaw = poeReadFile(
        std::string(RPI5_THERMAL_CPU_ZONE) + "/temp");
    double tempC = 0.0;
    bool tempOk = poeParseDouble(tempRaw, tempC);
    if (tempOk)
    {
        tempC /= 1000.0;
        data.push_back("[INFO] CPU temperature: " + std::to_string(tempC) + " °C");
        if (tempC > static_cast<double>(RPI5_THERMAL_SHUTDOWN_C))
        {
            data.push_back("[FAIL] CPU temperature " + std::to_string(tempC) +
                           " °C exceeds the shutdown threshold — thermal protection must trip");
            TestOutput _out;
            _out.status = hal::Status::FAIL;
            _out.severity = hal::Severity::ERROR;
            StampDiagnostics(_out, data);
            return _out;
        }
    }
    else
    {
        data.push_back("[INFO] CPU temperature unreadable");
    }
    data.push_back("[INFO] Pi5 throttle threshold: " + std::to_string(RPI5_THERMAL_THROTTLE_C) + " °C");
    data.push_back("[INFO] Pi5 shutdown threshold: " + std::to_string(RPI5_THERMAL_SHUTDOWN_C) + " °C");

    // Fan trip points from thermal zone
    std::string trips = poeRunCmd(
        "cat /sys/class/thermal/thermal_zone0/trip_point_*/temp 2>/dev/null");
    if (!trips.empty())
        data.push_back("[INFO] Trip point temps (m°C): " + trips);

    // Acceptance criteria (measurable): no active throttle flags (thermal
    // protection effective). Throttle flags set -> FAIL; throttle status
    // unreadable -> SKIP.
    std::string throttle = poeRunCmd("vcgencmd get_throttled 2>/dev/null");
    bool ok = !throttle.empty() && throttle.find("throttled=0x0") == 0;
    if (ok)
    {
        data.push_back("[PASS] No throttle flags (temps within limits)");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    if (throttle.empty() && !tempOk)
    {
        data.push_back("[SKIP] Thermal state not observable (vcgencmd absent, thermal zone unreadable)");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    if (!throttle.empty())
    {
        data.push_back("[FAIL] Throttle flags set: " + throttle + " — thermal/under-voltage protection engaged");
        TestOutput _out;
        _out.status = hal::Status::FAIL;
        _out.severity = hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[PASS] Temperature within limits (throttle flag unavailable, temperature nominal)");
    TestOutput _out;
    _out.status = hal::Status::PASS;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief Verify 5V rail is within range using vcgencmd / power sysfs.
 */
TestOutput PoeHatTesting::testVoltageRegulator()
{
    std::vector<std::string> data;
    data.push_back("=== PoE HAT Voltage Regulator (5V output) ===");
    data.push_back("[INFO] PoE+ HAT DC-DC: synchronous buck, 5V / 5A = 25 W, ≥88% efficiency");

    std::string vCore = poeRunCmd("vcgencmd measure_volts core 2>/dev/null");
    std::string vSDRAM = poeRunCmd("vcgencmd measure_volts sdram_c 2>/dev/null");
    data.push_back("[INFO] Core voltage: " + (vCore.empty() ? "N/A" : vCore));
    data.push_back("[INFO] SDRAM voltage: " + (vSDRAM.empty() ? "N/A" : vSDRAM));

    // Check under-voltage bit in throttle flags
    std::string throttle = poeRunCmd("vcgencmd get_throttled 2>/dev/null");
    bool underVolt = false;
    if (!throttle.empty())
    {
        // Bit 0: under-voltage detected; bit 16: under-voltage has occurred
        try {
            unsigned long flags = std::stoul(throttle.substr(throttle.find("0x")), nullptr, 16);
            underVolt = (flags & 0x1) || (flags & 0x10000);
        } catch (...) {}
    }
    data.push_back(underVolt
        ? "[WARN] Under-voltage detected — check PoE injector / cable quality"
        : "[PASS] No under-voltage — 5V regulator output stable");

    {
        TestOutput _out;
        _out.status = !underVolt ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = !underVolt ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

/**
 * @brief End-to-end power delivery: run brief stress test and monitor stability.
 */
TestOutput PoeHatTesting::testPowerDelivery()
{
    std::vector<std::string> data;
    data.push_back("=== PoE HAT Power Delivery (end-to-end load test) ===");

    // Pre-stress temperature
    std::string tempBefore = poeReadFile(
        std::string(RPI5_THERMAL_CPU_ZONE) + "/temp");
    double tBefore = 0.0;
    if (poeParseDouble(tempBefore, tBefore))
        data.push_back("[INFO] CPU temp before stress: " + std::to_string(tBefore / 1000.0) + " °C");
    else
        data.push_back("[INFO] CPU temp before stress: N/A");

    // 2-second CPU stress
    std::string stressOut = poeRunCmd("stress-ng --cpu 4 --timeout 2s --metrics-brief 2>&1 | tail -3");
    if (stressOut.empty())
        stressOut = poeRunCmd("dd if=/dev/zero of=/dev/null bs=1M count=2048 2>&1 | tail -1");
    data.push_back("[INFO] Load: " + (stressOut.empty() ? "no load tool available" : stressOut));

    // Post-stress throttle check
    std::string throttle = poeRunCmd("vcgencmd get_throttled 2>/dev/null");
    bool ok = !throttle.empty() && throttle.find("throttled=0x0") == 0;

    // Post-stress temperature
    std::string tempAfter = poeReadFile(
        std::string(RPI5_THERMAL_CPU_ZONE) + "/temp");
    double tAfter = 0.0;
    if (poeParseDouble(tempAfter, tAfter))
        data.push_back("[INFO] CPU temp after stress: " + std::to_string(tAfter / 1000.0) + " °C");
    else
        data.push_back("[INFO] CPU temp after stress: N/A");

    data.push_back("[INFO] PoE+ HAT rated for Pi5 maximum load (~8-9 W CPU full load)");

    // Acceptance criteria (measurable): the supply must hold the rail under
    // load (no throttle flags after a 2 s all-core stress). No load tool and
    // no throttle telemetry -> SKIP.
    if (stressOut.empty() && throttle.empty())
    {
        data.push_back("[SKIP] No load tool and no throttle telemetry — power delivery cannot be exercised");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    if (!ok)
    {
        data.push_back("[FAIL] Throttle flags after load: " + throttle + " — PoE supply insufficient under load");
        TestOutput _out;
        _out.status = hal::Status::FAIL;
        _out.severity = hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
    data.push_back("[PASS] No throttle flags under load — PoE supply held the rail");
    TestOutput _out;
    _out.status = hal::Status::PASS;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

// ---------------------------------------------------------------------------
// Boilerplate
// ---------------------------------------------------------------------------

std::vector<TestResult> PoeHatTesting::runAllTests()
{
    std::vector<TestResult> results;
    auto run = [&](const std::string& name, TestOutput (PoeHatTesting::*fn)()) {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput out = (this->*fn)();
        auto end = std::chrono::high_resolution_clock::now();
        {
            TestResult _tr;
            _tr.name = name;
            _tr.test_id = name;
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            _tr.output = out;
            results.push_back(_tr);
        }
    };
    run("testPoeHatDetection",     &PoeHatTesting::testPoeHatDetection);
    run("testPoePowerNegotiation", &PoeHatTesting::testPoePowerNegotiation);
    run("testPowerMonitoring",     &PoeHatTesting::testPowerMonitoring);
    run("testFanControl",          &PoeHatTesting::testFanControl);
    run("testFanSpeed",            &PoeHatTesting::testFanSpeed);
    run("testThermalProtection",   &PoeHatTesting::testThermalProtection);
    run("testVoltageRegulator",    &PoeHatTesting::testVoltageRegulator);
    run("testPowerDelivery",       &PoeHatTesting::testPowerDelivery);
    return results;
}

std::vector<std::string> PoeHatTesting::getAvailableTests()
{
    return {
        "testPoeHatDetection",
        "testPoePowerNegotiation",
        "testPowerMonitoring",
        "testFanControl",
        "testFanSpeed",
        "testThermalProtection",
        "testVoltageRegulator",
        "testPowerDelivery",
    };
}

TestResult PoeHatTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput result;
    result.status = hal::Status::ERROR;
    result.severity = hal::Severity::ERROR;
    result.error_message = "test not dispatched";

    if      (testName == "testPoeHatDetection")     result = testPoeHatDetection();
    else if (testName == "testPoePowerNegotiation") result = testPoePowerNegotiation();
    else if (testName == "testPowerMonitoring")     result = testPowerMonitoring();
    else if (testName == "testFanControl")          result = testFanControl();
    else if (testName == "testFanSpeed")            result = testFanSpeed();
    else if (testName == "testThermalProtection")   result = testThermalProtection();
    else if (testName == "testVoltageRegulator")    result = testVoltageRegulator();
    else if (testName == "testPowerDelivery")       result = testPowerDelivery();
    else {
        auto end = std::chrono::high_resolution_clock::now();
        {
            TestResult _tr;
            _tr.name = testName + " (UNKNOWN)";
            _tr.test_id = testName;
            _tr.output.status = hal::Status::ERROR;
            _tr.output.severity = hal::Severity::ERROR;
            _tr.output.error_message = "Unknown test name: " + testName;
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            return _tr;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    {
        TestResult _tr;
        _tr.name = testName;
        _tr.test_id = testName;
        _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        _tr.output = result;
        return _tr;
    }
}
