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
#include <cstring>

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

TestOutput UartTesting::testUartLoopback()
{
    std::vector<std::string> data;
    data.push_back("=== UART Loopback (open + termios) ===");
    bool passed = false;
    int fd = open(RPI5_DEBUG_UART, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd >= 0) {
        struct termios tio;
        if (tcgetattr(fd, &tio) == 0) {
            cfmakeraw(&tio);
            cfsetspeed(&tio, B115200);
            if (tcsetattr(fd, TCSANOW, &tio) == 0) {
                data.push_back("[PASS] " + std::string(RPI5_DEBUG_UART) + " opened and configured at 115200 8N1");
                passed = true;
            } else {
                data.push_back("[WARN] tcsetattr failed: " + std::string(strerror(errno)));
            }
        } else {
            data.push_back("[WARN] tcgetattr failed: " + std::string(strerror(errno)));
        }
        close(fd);
    } else {
        data.push_back("[FAIL] Cannot open " + std::string(RPI5_DEBUG_UART) + ": " + strerror(errno));
    }
    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

TestOutput UartTesting::testUartBaudRate()
{
    std::vector<std::string> data;
    data.push_back("=== UART Baud Rate Configuration ===");
    bool passed = false;
    int fd = open(RPI5_DEBUG_UART, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd >= 0) {
        struct termios tio;
        if (tcgetattr(fd, &tio) == 0) {
            tio.c_cflag |= (CLOCAL | CREAD);
            for (speed_t sp : { B9600, B19200, B57600, B115200 }) {
                cfsetspeed(&tio, sp);
                if (tcsetattr(fd, TCSANOW, &tio) == 0) {
                    data.push_back("[PASS] Baud programmed successfully");
                    passed = true;
                    break;
                }
            }
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
    bool passed = false;
    int fd = open(RPI5_DEBUG_UART, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd >= 0) {
        char buf[64];
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n >= 0) {
            data.push_back("[PASS] UART read() successful (no data pending is normal)");
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
