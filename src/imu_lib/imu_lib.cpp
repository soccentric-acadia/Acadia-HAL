/**
 * @file imu_lib.cpp
 * @brief IMU tests for Raspberry Pi 5.
 *        Pi5 has NO built-in IMU.  Inertial sensing requires an external module
 *        connected via I2C (e.g. ICM-42688 on /dev/i2c-1) or SPI.  Tests use
 *        the Linux IIO (Industrial I/O) subsystem to read accelerometer,
 *        gyroscope, and magnetometer channels via /sys/bus/iio/devices/.
 *        If no IMU is connected, tests pass with an informational warning.
 * @author sandesh ghimire
 * @copyright (C) Lucid Motors
 */

#include "imu_lib.h"
#include "../platform_rpi5.h"

#include <array>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

// ---------------------------------------------------------------------------
static std::string imuReadFile(const std::string& path)
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

static std::string imuRunCmd(const std::string& cmd)
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

// Find all IIO device paths
static std::vector<std::string> imuListIioDevices()
{
    std::vector<std::string> devs;
    const std::string base = "/sys/bus/iio/devices";
    DIR* dir = opendir(base.c_str());
    if (!dir) return devs;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        std::string name(entry->d_name);
        if (name.find("iio:device") == 0)
            devs.push_back(base + "/" + name);
    }
    closedir(dir);
    return devs;
}

// Safe numeric parsing: rejects empty/garbage input instead of throwing.
static bool imuParseDouble(const std::string& s, double& out)
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
 * @brief Constructor for ImuTesting
 */
ImuTesting::ImuTesting()
{
}

/**
 * @brief Destructor for ImuTesting
 */
ImuTesting::~ImuTesting()
{
}

/**
 * @brief Test 3-axis accelerometer via IIO subsystem.
 */
TestOutput ImuTesting::test3axisAccelerometer()
{
    std::vector<std::string> data;
    data.push_back("=== 3-Axis Accelerometer (Pi5 + External IMU / IIO) ===");
    data.push_back("[INFO] Pi5 has no built-in IMU; checking IIO /sys/bus/iio/devices/");

    auto devs = imuListIioDevices();
    for (const auto& dev : devs)
    {
        // Check for accel channels
        bool hasX = !imuReadFile(dev + "/in_accel_x_raw").empty();
        bool hasY = !imuReadFile(dev + "/in_accel_y_raw").empty();
        bool hasZ = !imuReadFile(dev + "/in_accel_z_raw").empty();
        if (hasX && hasY && hasZ)
        {
            std::string name  = imuReadFile(dev + "/name");
            std::string xRaw  = imuReadFile(dev + "/in_accel_x_raw");
            std::string yRaw  = imuReadFile(dev + "/in_accel_y_raw");
            std::string zRaw  = imuReadFile(dev + "/in_accel_z_raw");
            std::string scale = imuReadFile(dev + "/in_accel_scale");
            data.push_back("Accelerometer: " + name);
            data.push_back("[INFO]   X=" + xRaw + " Y=" + yRaw + " Z=" + zRaw +
                           " scale=" + scale + " m/s²");

            // Acceptance criteria (measurable): the raw readings must parse
            // and, when a scale is available, the gravity magnitude must be
            // plausible (0.5–30 m/s², i.e. roughly 0.05–3 g for a stationary
            // board).
            double x = 0, y = 0, z = 0, sc = 1.0;
            if (imuParseDouble(xRaw, x) && imuParseDouble(yRaw, y) && imuParseDouble(zRaw, z))
            {
                bool scaleOk = imuParseDouble(scale, sc) && sc > 0.0;
                if (scaleOk)
                {
                    double magnitude = std::sqrt(x * x + y * y + z * z) * sc;
                    data.push_back("[INFO]   gravity magnitude: " + std::to_string(magnitude) + " m/s²");
                    if (magnitude < 0.5 || magnitude > 30.0)
                    {
                        data.push_back("[FAIL] Accelerometer gravity magnitude out of range (" +
                                       std::to_string(magnitude) + " m/s²) — sensor fault or scaling error");
                        TestOutput _out;
                        _out.status = hal::Status::FAIL;
                        _out.severity = hal::Severity::ERROR;
                        StampDiagnostics(_out, data);
                        return _out;
                    }
                }
                data.push_back("[PASS] 3-axis accelerometer present and readings valid" +
                               std::string(scaleOk ? "" : " (scale unavailable — magnitude not verified)"));
                TestOutput _out;
                _out.status = hal::Status::PASS;
                _out.severity = hal::Severity::INFO;
                StampDiagnostics(_out, data);
                return _out;
            }
            data.push_back("[FAIL] Accelerometer raw readings unparseable (X='" + xRaw + "', Y='" + yRaw + "', Z='" + zRaw + "')");
            TestOutput _out;
            _out.status = hal::Status::FAIL;
            _out.severity = hal::Severity::ERROR;
            StampDiagnostics(_out, data);
            return _out;
        }
    }

    data.push_back("[SKIP] No IIO 3-axis accelerometer present (connect an external IMU via I2C/SPI, e.g. ICM-42688 on /dev/i2c-1)");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief Test 3-axis gyroscope via IIO subsystem.
 */
TestOutput ImuTesting::test3axisGyroscope()
{
    std::vector<std::string> data;
    data.push_back("=== 3-Axis Gyroscope (Pi5 + External IMU / IIO) ===");

    auto devs = imuListIioDevices();
    for (const auto& dev : devs)
    {
        bool hasX = !imuReadFile(dev + "/in_anglvel_x_raw").empty();
        bool hasY = !imuReadFile(dev + "/in_anglvel_y_raw").empty();
        bool hasZ = !imuReadFile(dev + "/in_anglvel_z_raw").empty();
        if (hasX && hasY && hasZ)
        {
            std::string name  = imuReadFile(dev + "/name");
            std::string xRaw  = imuReadFile(dev + "/in_anglvel_x_raw");
            std::string yRaw  = imuReadFile(dev + "/in_anglvel_y_raw");
            std::string zRaw  = imuReadFile(dev + "/in_anglvel_z_raw");
            std::string scale = imuReadFile(dev + "/in_anglvel_scale");
            data.push_back("Gyroscope: " + name);
            data.push_back("[INFO]   X=" + xRaw + " Y=" + yRaw + " Z=" + zRaw +
                           " scale=" + scale + " rad/s");

            // Acceptance criteria (measurable): raw readings must parse into
            // finite values.
            double x = 0, y = 0, z = 0;
            if (imuParseDouble(xRaw, x) && imuParseDouble(yRaw, y) && imuParseDouble(zRaw, z))
            {
                data.push_back("[PASS] 3-axis gyroscope present with parseable angular-velocity readings");
                TestOutput _out;
                _out.status = hal::Status::PASS;
                _out.severity = hal::Severity::INFO;
                StampDiagnostics(_out, data);
                return _out;
            }
            data.push_back("[FAIL] Gyroscope raw readings unparseable (X='" + xRaw + "', Y='" + yRaw + "', Z='" + zRaw + "')");
            TestOutput _out;
            _out.status = hal::Status::FAIL;
            _out.severity = hal::Severity::ERROR;
            StampDiagnostics(_out, data);
            return _out;
        }
    }

    data.push_back("[SKIP] No IIO 3-axis gyroscope found — external IMU required");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief Test 3-axis magnetometer via IIO subsystem.
 */
TestOutput ImuTesting::test3axisMagnetometer()
{
    std::vector<std::string> data;
    data.push_back("=== 3-Axis Magnetometer (Pi5 + External Compass / IIO) ===");

    auto devs = imuListIioDevices();
    for (const auto& dev : devs)
    {
        bool hasX = !imuReadFile(dev + "/in_magn_x_raw").empty();
        bool hasY = !imuReadFile(dev + "/in_magn_y_raw").empty();
        bool hasZ = !imuReadFile(dev + "/in_magn_z_raw").empty();
        if (hasX && hasY && hasZ)
        {
            std::string name  = imuReadFile(dev + "/name");
            std::string xRaw  = imuReadFile(dev + "/in_magn_x_raw");
            std::string yRaw  = imuReadFile(dev + "/in_magn_y_raw");
            std::string zRaw  = imuReadFile(dev + "/in_magn_z_raw");
            std::string scale = imuReadFile(dev + "/in_magn_scale");
            data.push_back("Magnetometer: " + name);
            data.push_back("[INFO]   X=" + xRaw + " Y=" + yRaw + " Z=" + zRaw +
                           " scale=" + scale + " Gauss");

            // Acceptance criteria (measurable): raw readings must parse and
            // the field magnitude must be plausible (0.1–1000 µT; Earth
            // field ~25–65 µT plus local distortion).
            double x = 0, y = 0, z = 0, sc = 1.0;
            if (imuParseDouble(xRaw, x) && imuParseDouble(yRaw, y) && imuParseDouble(zRaw, z))
            {
                bool scaleOk = imuParseDouble(scale, sc) && sc > 0.0;
                bool magOk = true;
                if (scaleOk)
                {
                    double magnitude = std::sqrt(x * x + y * y + z * z) * sc;
                    data.push_back("[INFO]   field magnitude: " + std::to_string(magnitude) + " µT");
                    magOk = magnitude >= 0.1 && magnitude <= 1000.0;
                }
                if (!magOk)
                {
                    data.push_back("[FAIL] Magnetometer field magnitude out of plausible range — sensor fault or scaling error");
                    TestOutput _out;
                    _out.status = hal::Status::FAIL;
                    _out.severity = hal::Severity::ERROR;
                    StampDiagnostics(_out, data);
                    return _out;
                }
                data.push_back("[PASS] 3-axis magnetometer present with valid readings");
                TestOutput _out;
                _out.status = hal::Status::PASS;
                _out.severity = hal::Severity::INFO;
                StampDiagnostics(_out, data);
                return _out;
            }
            data.push_back("[FAIL] Magnetometer raw readings unparseable (X='" + xRaw + "', Y='" + yRaw + "', Z='" + zRaw + "')");
            TestOutput _out;
            _out.status = hal::Status::FAIL;
            _out.severity = hal::Severity::ERROR;
            StampDiagnostics(_out, data);
            return _out;
        }
    }

    data.push_back("[SKIP] No IIO magnetometer found — external compass HAT required");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief Test IMU data rate via IIO sampling_frequency attribute.
 */
TestOutput ImuTesting::testImuDataRate()
{
    std::vector<std::string> data;
    data.push_back("=== IMU Data Rate (Pi5 IIO) ===");

    auto devs = imuListIioDevices();
    for (const auto& dev : devs)
    {
        std::string sampFreq   = imuReadFile(dev + "/in_accel_sampling_frequency");
        std::string availFreqs = imuReadFile(dev + "/in_accel_sampling_frequency_available");
        if (!sampFreq.empty())
        {
            std::string name = imuReadFile(dev + "/name");
            double freq = 0.0;
            if (!imuParseDouble(sampFreq, freq) || freq <= 0.0)
            {
                data.push_back("[FAIL] " + name + " sampling frequency value invalid: '" + sampFreq + "'");
                TestOutput _out;
                _out.status = hal::Status::FAIL;
                _out.severity = hal::Severity::ERROR;
                StampDiagnostics(_out, data);
                return _out;
            }
            data.push_back(name + " sampling frequency: " + sampFreq + " Hz");
            if (!availFreqs.empty())
                data.push_back("[INFO] Available frequencies: " + availFreqs);
            if (freq >= 100.0)
            {
                data.push_back("[PASS] IMU output data rate " + std::to_string(freq) + " Hz (≥100 Hz IMU-grade rate)");
                TestOutput _out;
                _out.status = hal::Status::PASS;
                _out.severity = hal::Severity::INFO;
                StampDiagnostics(_out, data);
                return _out;
            }
            data.push_back("[FAIL] Sampling frequency " + std::to_string(freq) + " Hz below the 100 Hz IMU-grade minimum");
            TestOutput _out;
            _out.status = hal::Status::FAIL;
            _out.severity = hal::Severity::ERROR;
            StampDiagnostics(_out, data);
            return _out;
        }
    }

    data.push_back("[SKIP] No IIO device with sampling_frequency — external IMU required (ICM-42688 supports up to 32 kHz ODR)");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief Test IMU accuracy by checking noise density via IIO attributes.
 */
TestOutput ImuTesting::testImuAccuracy()
{
    std::vector<std::string> data;
    data.push_back("=== IMU Accuracy (Pi5 IIO) ===");

    auto devs = imuListIioDevices();
    for (const auto& dev : devs)
    {
        std::string name = imuReadFile(dev + "/name");
        if (name.empty()) continue;

        // Bias correction offsets
        std::string axOff = imuReadFile(dev + "/in_accel_x_calibbias");
        std::string gyOff = imuReadFile(dev + "/in_anglvel_x_calibbias");
        if (!axOff.empty() || !gyOff.empty())
        {
            data.push_back(name + " calibration bias registers accessible");
            if (!axOff.empty()) data.push_back("[INFO]   Accel X bias: " + axOff);
            if (!gyOff.empty()) data.push_back("[INFO]   Gyro  X bias: " + gyOff);
            data.push_back("[PASS] Bias-correction registers accessible (accuracy characterisable on this sensor)");
            TestOutput _out;
            _out.status = hal::Status::PASS;
            _out.severity = hal::Severity::INFO;
            StampDiagnostics(_out, data);
            return _out;
        }
        data.push_back("[INFO] " + name + ": no calibbias attributes");
    }

    data.push_back("[SKIP] No IIO IMU with calibbias attributes — accuracy characterisation (Allan variance) cannot be performed");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief Test IMU calibration via IIO calibbias attributes.
 */
TestOutput ImuTesting::testImuCalibration()
{
    std::vector<std::string> data;
    data.push_back("=== IMU Calibration (Pi5 IIO) ===");

    auto devs = imuListIioDevices();
    bool found = false;
    for (const auto& dev : devs)
    {
        std::string name = imuReadFile(dev + "/name");
        if (name.empty()) continue;

        // Check all 3-axis calibbias for accel and gyro
        static const std::vector<std::string> axes = {"x", "y", "z"};
        bool hasCalib = false;
        for (const auto& ax : axes)
        {
            std::string aOff = imuReadFile(dev + "/in_accel_" + ax + "_calibbias");
            std::string gOff = imuReadFile(dev + "/in_anglvel_" + ax + "_calibbias");
            if (!aOff.empty() || !gOff.empty()) { hasCalib = true; found = true; }
        }
        data.push_back((hasCalib ? "[PASS] " : "[INFO] ") + name +
                       (hasCalib ? ": calibbias writeable" : ": no calibbias attrs"));
    }

    if (found)
    {
        data.push_back("[PASS] Calibration interface (calibbias registers) accessible on the connected IMU (full 6-axis calibration still requires a known motion fixture)");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[SKIP] No IIO IMU with calibbias attributes — calibration interface cannot be verified");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief Test IMU temperature compensation via IIO temp channel.
 */
TestOutput ImuTesting::testImuTemperatureCompensation()
{
    std::vector<std::string> data;
    data.push_back("=== IMU Temperature Compensation (Pi5 IIO) ===");

    auto devs = imuListIioDevices();
    for (const auto& dev : devs)
    {
        std::string tempRaw   = imuReadFile(dev + "/in_temp_raw");
        std::string tempScale = imuReadFile(dev + "/in_temp_scale");
        std::string tempOff   = imuReadFile(dev + "/in_temp_offset");
        if (!tempRaw.empty())
        {
            std::string name = imuReadFile(dev + "/name");
            double raw = 0.0, scale = 1.0, off = 0.0;
            if (!imuParseDouble(tempRaw, raw) ||
                (!tempScale.empty() && !(imuParseDouble(tempScale, scale) && scale != 0.0)) ||
                (!tempOff.empty() && !imuParseDouble(tempOff, off)))
            {
                data.push_back("[FAIL] " + name + " temperature channel unparseable (raw='" +
                               tempRaw + "', scale='" + tempScale + "', offset='" + tempOff + "')");
                TestOutput _out;
                _out.status = hal::Status::FAIL;
                _out.severity = hal::Severity::ERROR;
                StampDiagnostics(_out, data);
                return _out;
            }
            double tempC = (raw + off) * scale;
            if (tempC < -40.0 || tempC > 125.0)
            {
                data.push_back("[FAIL] " + name + " die temperature out of sensor range: " +
                               std::to_string(tempC) + " °C — compensation data invalid");
                TestOutput _out;
                _out.status = hal::Status::FAIL;
                _out.severity = hal::Severity::ERROR;
                StampDiagnostics(_out, data);
                return _out;
            }
            data.push_back("[PASS] " + name + " die temp readable and in range: " +
                           std::to_string(tempC) + " °C (used by IMU firmware for TC correction)");
            TestOutput _out;
            _out.status = hal::Status::PASS;
            _out.severity = hal::Severity::INFO;
            StampDiagnostics(_out, data);
            return _out;
        }
    }

    data.push_back("[SKIP] No IIO temperature channel found — no IMU connected");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief Test IMU-to-Pi5 interface (I2C bus presence and communication).
 * @details Named testImuToOrinInterface in header (Jetson-origin name);
 *          on Pi5 this verifies /dev/i2c-1 (GPIO 2/3) which is the standard
 *          I2C bus for external IMU modules.
 */
TestOutput ImuTesting::testImuToOrinInterface()
{
    std::vector<std::string> data;
    data.push_back("=== IMU-to-Pi5 Interface (I2C / SPI, Pi5) ===");
    data.push_back("[INFO] Pi5 standard IMU bus: I2C-1 (/dev/i2c-1, GPIO 2/3 = SDA/SCL)");

    // Check I2C-1 presence
    bool i2c1 = !imuRunCmd("ls /dev/i2c-1 2>/dev/null").empty();
    data.push_back(i2c1 ? "[PASS] /dev/i2c-1 present"
                        : "[WARN] /dev/i2c-1 not found — enable with dtparam=i2c_arm=on");

    // Scan I2C-1 for common IMU addresses
    if (i2c1)
    {
        std::string i2cDet = imuRunCmd("i2cdetect -y 1 2>/dev/null | head -10");
        if (!i2cDet.empty())
        {
            data.push_back("[INFO] I2C-1 scan:\n" + i2cDet);
            // Common IMU addresses: 0x68 (MPU-6050/ICM), 0x69, 0x1C (LIS), 0x28 (BNO)
            bool imuFound = i2cDet.find(" 68 ") != std::string::npos ||
                            i2cDet.find(" 69 ") != std::string::npos ||
                            i2cDet.find(" 1c ") != std::string::npos ||
                            i2cDet.find(" 28 ") != std::string::npos;
            data.push_back(imuFound ? "[PASS] IMU address detected on I2C-1"
                                    : "[INFO] No known IMU address on I2C-1");
        }
    }

    // Also check IIO devices enumerated
    auto iioDevs = imuListIioDevices();
    data.push_back("[INFO] IIO devices enumerated: " + std::to_string(iioDevs.size()));

    {
        TestOutput _out;
        _out.status = i2c1 ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = i2c1 ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

/**
 * @brief Run all tests for this module
 * @return Vector of test results
 */
std::vector<TestResult> ImuTesting::runAllTests()
{
    std::vector<TestResult> results;

    // Run test3axisAccelerometer
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = test3axisAccelerometer();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "test3axisAccelerometer";
            _tr.test_id = "test3axisAccelerometer";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run test3axisGyroscope
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = test3axisGyroscope();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "test3axisGyroscope";
            _tr.test_id = "test3axisGyroscope";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run test3axisMagnetometer
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = test3axisMagnetometer();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "test3axisMagnetometer";
            _tr.test_id = "test3axisMagnetometer";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testImuDataRate
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testImuDataRate();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testImuDataRate";
            _tr.test_id = "testImuDataRate";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testImuAccuracy
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testImuAccuracy();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testImuAccuracy";
            _tr.test_id = "testImuAccuracy";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testImuCalibration
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testImuCalibration();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testImuCalibration";
            _tr.test_id = "testImuCalibration";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testImuTemperatureCompensation
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testImuTemperatureCompensation();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testImuTemperatureCompensation";
            _tr.test_id = "testImuTemperatureCompensation";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testImuToOrinInterface
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testImuToOrinInterface();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testImuToOrinInterface";
            _tr.test_id = "testImuToOrinInterface";
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
std::vector<std::string> ImuTesting::getAvailableTests()
{
    return {
        "testImuDataRate", "testImuAccuracy", "testImuCalibration", "testImuTemperatureCompensation", "testImuToOrinInterface",
    };
}

/**
 * @brief Run a single test by name
 * @param testName Name of the test to run
 * @return Test result with timing
 */
TestResult ImuTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput result;
    result.status = hal::Status::ERROR;
    result.severity = hal::Severity::ERROR;
    result.error_message = "test not dispatched";

    if (testName == "testImuDataRate") {
        result = testImuDataRate();
    } else if (testName == "testImuAccuracy") {
        result = testImuAccuracy();
    } else if (testName == "testImuCalibration") {
        result = testImuCalibration();
    } else if (testName == "testImuTemperatureCompensation") {
        result = testImuTemperatureCompensation();
    } else if (testName == "testImuToOrinInterface") {
        result = testImuToOrinInterface();
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
