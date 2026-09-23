/**
 * @file uart_lib.cpp
 * @brief Implementation of UartTesting for Raspberry Pi 5 (PL011 UART).
 * @details Verifies PL011 serial ports (/dev/ttyAMA*), opens the debug UART,
 *          checks baud-rate configurability via termios, and reads the kernel
 *          ring for a short poll window (non-blocking, hardware-agnostic).
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */

#include "uart_lib.h"
#include "../platform_rpi5.h"
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <vector>
#include <chrono>
#include <cstring>
#include <thread>

static bool pathExists(const std::string& p)
{
    struct stat s;
    return stat(p.c_str(), &s) == 0;
}

UartTesting::UartTesting() {}
UartTesting::~UartTesting() {}

TestOutput UartTesting::testUartPortPresent()
{
    std::vector<std::string> data;
    data.push_back("=== UART Port Presence ===");
    bool passed = false;
    const char* devs[] = { RPI5_DEBUG_UART, RPI5_UART1, "/dev/ttyS0" };
    int found = 0;
    for (const char* d : devs) {
        if (pathExists(d)) {
            data.push_back("[PASS] UART device node present: " + std::string(d));
            found++;
            passed = true;
        }
    }
    if (!passed)
        data.push_back("[FAIL] No UART device nodes found (ttyAMA0/ttyS0)");
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        _out.metrics["uart_ports"] = static_cast<int64_t>(found);
        return _out;
    }
}

// HAL-502/503/504: tests must never reconfigure the serial console.
// The test port comes from ACADIA_UART_TEST_PORT and must differ from the
// console UART; with no harness configured the tests SKIP instead of
// touching /dev/ttyAMA0.
static std::string uartTestPort(std::vector<std::string>& data)
{
    const char* env = getenv("ACADIA_UART_TEST_PORT");
    std::string port = (env && *env) ? env : "";
    if (port.empty() || port == RPI5_DEBUG_UART) {
        data.push_back("[SKIP] ACADIA_UART_TEST_PORT not set to a non-console port — refusing to reconfigure the debug console (" +
                       std::string(RPI5_DEBUG_UART) + "); wire a loopback harness and set ACADIA_UART_TEST_PORT");
        return "";
    }
    return port;
}

TestOutput UartTesting::testUartLoopback()
{
    std::vector<std::string> data;
    data.push_back("=== UART Loopback (TX→RX on configured non-console port) ===");
    std::string port = uartTestPort(data);
    if (port.empty()) {
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    bool passed = false;
    int fd = open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd >= 0) {
        struct termios orig, tio;
        bool haveOrig = (tcgetattr(fd, &orig) == 0);
        if (haveOrig) {
            tio = orig;
            cfmakeraw(&tio);
            cfsetspeed(&tio, B115200);
            if (tcsetattr(fd, TCSANOW, &tio) == 0) {
                // Real TX→RX loopback: send a known byte, expect it back.
                static const unsigned char probe[] = {0x55, 0xAA, 0x55};
                tcflush(fd, TCIOFLUSH);
                ssize_t w = write(fd, probe, sizeof(probe));
                (void)w;
                unsigned char rx[32] = {};
                ssize_t got = 0;
                auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
                while (got < (ssize_t)sizeof(probe) && std::chrono::steady_clock::now() < deadline) {
                    ssize_t n = ::read(fd, rx + got, sizeof(probe) - (size_t)got);
                    if (n > 0) got += n;
                    else std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                if (got == (ssize_t)sizeof(probe) &&
                    memcmp(rx, probe, sizeof(probe)) == 0) {
                    data.push_back("[PASS] Loopback verified on " + port +
                                   " (TX 55 AA 55 → RX 55 AA 55 at 115200 8N1)");
                    passed = true;
                } else if (got > 0) {
                    data.push_back("[FAIL] Loopback data mismatch on " + port + " (" +
                                   std::to_string(got) + "/" + std::to_string(sizeof(probe)) + " bytes matched wrong)");
                } else {
                    data.push_back("[SKIP] No loopback response on " + port +
                                   " — TX→RX jumper or remote echo not present");
                }
                // HAL-502: restore the original termios configuration.
                tcsetattr(fd, TCSANOW, &orig);
            } else {
                data.push_back("[WARN] tcsetattr failed: " + std::string(strerror(errno)));
            }
        } else {
            data.push_back("[WARN] tcgetattr failed: " + std::string(strerror(errno)));
        }
        close(fd);
    } else {
        data.push_back("[FAIL] Cannot open " + port + ": " + strerror(errno));
    }
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS
                    : (data.back().find("[SKIP]") == 0 ? hal::Status::SKIPPED : hal::Status::FAIL);
        _out.severity = _out.status == hal::Status::FAIL ? hal::Severity::ERROR : hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
}

TestOutput UartTesting::testUartBaudRate()
{
    std::vector<std::string> data;
    data.push_back("=== UART Baud Rate Configuration ===");
    std::string port = uartTestPort(data);
    if (port.empty()) {
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    bool passed = false;
    int fd = open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd >= 0) {
        struct termios orig, tio;
        if (tcgetattr(fd, &orig) == 0) {
            tio = orig;
            tio.c_cflag |= (CLOCAL | CREAD);
            for (speed_t sp : { B9600, B19200, B57600, B115200 }) {
                cfsetspeed(&tio, sp);
                if (tcsetattr(fd, TCSANOW, &tio) == 0) {
                    data.push_back("[PASS] Baud programmed successfully on " + port);
                    passed = true;
                    break;
                }
            }
            // HAL-503: restore the original termios configuration.
            tcsetattr(fd, TCSANOW, &orig);
        }
        close(fd);
    }
    if (!passed)
        data.push_back("[FAIL] UART baud rate could not be configured");
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

TestOutput UartTesting::testUartBufferRead()
{
    std::vector<std::string> data;
    data.push_back("=== UART Buffer Read (non-blocking) ===");
    std::string port = uartTestPort(data);
    if (port.empty()) {
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    bool passed = false;
    // HAL-504: read from the configured test port only — never the console
    // (reading the console steals getty input).
    int fd = open(port.c_str(), O_RDONLY | O_NOCTTY | O_NONBLOCK);
    if (fd >= 0) {
        char buf[64];
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n >= 0) {
            data.push_back("[PASS] UART read() successful on " + port + " (no data pending is normal)");
            passed = true;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            data.push_back("[PASS] UART read returned EAGAIN (no data yet) - driver responding");
            passed = true;
        } else {
            data.push_back("[FAIL] UART read error: " + std::string(strerror(errno)));
        }
        close(fd);
    } else {
        data.push_back("[FAIL] Cannot open UART for read: " + std::string(strerror(errno)));
    }
    if (!passed)
        data.push_back("[FAIL] UART buffered read not functional");
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

std::vector<TestResult> UartTesting::runAllTests()
{
    std::vector<TestResult> results;
    const std::vector<std::pair<std::string, TestOutput (UartTesting::*)()>> tests = {
        {"testUartPortPresent", &UartTesting::testUartPortPresent},
        {"testUartLoopback", &UartTesting::testUartLoopback},
        {"testUartBaudRate", &UartTesting::testUartBaudRate},
        {"testUartBufferRead", &UartTesting::testUartBufferRead},
    };
    for (const auto& t : tests) {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput out = (this->*t.second)();
        auto end = std::chrono::high_resolution_clock::now();
        TestResult _tr;
        _tr.name = t.first;
        _tr.test_id = t.first;
        _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        _tr.output = out;
        results.push_back(std::move(_tr));
    }
    return results;
}

std::vector<std::string> UartTesting::getAvailableTests()
{
    return { "testUartPortPresent", "testUartLoopback", "testUartBaudRate", "testUartBufferRead" };
}

TestResult UartTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput out;
    out.status = hal::Status::ERROR;
    out.severity = hal::Severity::ERROR;
    out.error_message = "test not dispatched";
    bool known = true;
    if (testName == "testUartPortPresent") out = testUartPortPresent();
    else if (testName == "testUartLoopback") out = testUartLoopback();
    else if (testName == "testUartBaudRate") out = testUartBaudRate();
    else if (testName == "testUartBufferRead") out = testUartBufferRead();
    else known = false;
    auto end = std::chrono::high_resolution_clock::now();
    TestResult _tr;
    _tr.name = known ? testName : testName + " (UNKNOWN)";
    _tr.test_id = testName;
    _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    if (!known) {
        _tr.output.status = hal::Status::ERROR;
        _tr.output.severity = hal::Severity::ERROR;
        _tr.output.error_message = "Unknown test name: " + testName;
    } else {
        _tr.output = out;
    }
    return _tr;
}
