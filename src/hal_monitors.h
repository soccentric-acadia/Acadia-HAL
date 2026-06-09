/**
 * @file hal_monitors.h
 * @brief Shared registry of hardware monitor readers (Raspberry Pi 5)
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC
 *
 * Replaces the MONITORING RPC stub in protos/server/typed_module_handler.h
 * (which used to return "<name>:0" for every requested item) and the
 * duplicated trPi5MonitorItems() in src/test_runner.h. Both call sites
 * now resolve monitor names through hal::monitors::Lookup() against the
 * same name→reader map.
 *
 * Adding a new Pi 5 monitor:
 *   1. Append a new entry to the map in Pi5MonitorReaders() below.
 *   2. The monitor is now available to:
 *        - test_runner.h CLI binaries via trPi5MonitorItems() (unchanged API).
 *        - the gRPC MONITORING RPC via the typed_module_handler name lookup.
 *      No other file needs to change.
 *
 * Unknown names return a "N/A" reader, surfaced in the snapshot so
 * clients can distinguish "value is N/A" from "monitor is broken".
 */
#pragma once

#include "lib_base.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>

namespace hal {
namespace monitors {

// Read a sysfs file into a trimmed string. Empty string on error.
inline std::string ReadSysfs(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::string line;
    std::getline(f, line);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) {
        line.pop_back();
    }
    return line;
}

// Look up a single monitor item by name.
//
// Returns a MonitorItem with a reader that produces "N/A" for any
// unknown name. The "N/A" sentinel is the same one the legacy
// trPi5MonitorItems() readers returned on read failure, so consumers
// (test runner + gRPC envelope) treat both cases identically.
inline ::MonitorItem Lookup(const std::string& name)
{
    static const std::unordered_map<std::string, std::function<std::string()>> kReaders = {
        {"cpu_temp_C", []() {
            std::string v = ReadSysfs("/sys/class/thermal/thermal_zone0/temp");
            if (v.empty()) return std::string("N/A");
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(1) << (std::stod(v) / 1000.0);
            return ss.str();
        }},
        {"cpu0_freq_MHz", []() {
            std::string v = ReadSysfs("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
            if (v.empty()) return std::string("N/A");
            return std::to_string(std::stoll(v) / 1000);
        }},
        {"gpu_freq_MHz", []() {
            std::string v = ReadSysfs("/sys/class/devfreq/fde00000.gpu/cur_freq");
            if (v.empty()) return std::string("N/A");
            return std::to_string(std::stoll(v) / 1000000);
        }},
        {"mem_avail_MB", []() {
            std::ifstream f("/proc/meminfo");
            if (!f.is_open()) return std::string("N/A");
            std::string line;
            while (std::getline(f, line)) {
                if (line.rfind("MemAvailable:", 0) == 0) {
                    std::istringstream ss(line.substr(13));
                    long long kb = 0;
                    ss >> kb;
                    return std::to_string(kb / 1024) + " MB";
                }
            }
            return std::string("N/A");
        }},
    };

    auto it = kReaders.find(name);
    if (it == kReaders.end()) {
        return {name, [name]() -> std::string { return "N/A"; }};
    }
    return {name, it->second};
}

// All built-in Pi 5 monitors. Used by test_runner.h for the
// "monitor-test" CLI mode.
inline std::vector<::MonitorItem> AllPi5()
{
    return {
        Lookup("cpu_temp_C"),
        Lookup("cpu0_freq_MHz"),
        Lookup("gpu_freq_MHz"),
        Lookup("mem_avail_MB"),
    };
}

} // namespace monitors
} // namespace hal
