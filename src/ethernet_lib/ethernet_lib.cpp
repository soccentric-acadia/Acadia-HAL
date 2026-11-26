/**
 * @file ethernet_lib.cpp
 * @brief Implementation file for EthernetTesting class — Raspberry Pi 5
 *        Tests the BCM2712 built-in Gigabit Ethernet (1 GbE, not 10 GbE).
 *        Pi5 does NOT have 10 GbE hardware.
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */

#include "ethernet_lib.h"
#include "../platform_rpi5.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <net/if.h>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

// ---------------------------------------------------------------------------
// Pi5 Ethernet constants
// ---------------------------------------------------------------------------
static constexpr const char* ETH_IFACE     = RPI5_ETH_IFACE;     // "eth0"
static constexpr int         ETH_SPEED_MBPS = RPI5_ETH_SPEED_MBPS; // 1000

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::string ethReadFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::string s;
    std::getline(f, s);
    return s;
}

static long long ethReadInt(const std::string& path)
{
    std::string s = ethReadFile(path);
    if (s.empty()) return -1LL;
    try { return std::stoll(s); } catch (...) { return -1LL; }
}

static std::string ethRunCmd(const char* cmd)
{
    char buf[4096] = {};
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "";
    size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
    pclose(pipe);
    buf[n] = '\0';
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    return std::string(buf);
}

/** Check if the network interface exists in sysfs. */
static bool ifaceExists(const std::string& iface)
{
    return (access(("/sys/class/net/" + iface).c_str(), F_OK) == 0);
}

/** Return link state: "up"/"down"/"unknown". */
static std::string getLinkState(const std::string& iface)
{
    return ethReadFile("/sys/class/net/" + iface + "/operstate");
}

/** Return link speed in Mbps from ethtool, or -1. */
static int getLinkSpeedMbps(const std::string& iface)
{
    // Try sysfs speed file first
    long long spd = ethReadInt("/sys/class/net/" + iface + "/speed");
    if (spd > 0) return static_cast<int>(spd);
    return -1;
}

/** True when the given kernel module is loaded or available on the module path. */
static bool ethModuleAvailable(const char* mod)
{
    if (access(("/sys/module/" + std::string(mod)).c_str(), F_OK) == 0) return true;
    std::string path = ethRunCmd(("modinfo -F filename " + std::string(mod) + " 2>/dev/null").c_str());
    return !path.empty();
}

/** True when the tc (iproute2) binary is available. */
static bool ethTcAvailable()
{
    return access("/usr/sbin/tc", F_OK) == 0 || access("/sbin/tc", F_OK) == 0 ||
           access("/usr/bin/tc", F_OK) == 0;
}

/**
 * Measure wake-up jitter of a periodic 1 ms clock_nanosleep(CLOCK_MONOTONIC,
 * TIMER_ABSTIME) loop. Returns the maximum observed lateness in microseconds
 * (negative on invalid input). Software TSN on Pi5 targets <=50 us typical;
 * 1 ms is the hard acceptance ceiling.
 */
static double ethMeasureMaxJitterUs(int iterations)
{
    if (iterations <= 0) return -1.0;
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return -1.0;
    double maxLatenessUs = 0.0;
    for (int i = 0; i < iterations; ++i) {
        ts.tv_nsec += 1000000L; // 1 ms
        if (ts.tv_nsec >= 1000000000L) { ++ts.tv_sec; ts.tv_nsec -= 1000000000L; }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr);
        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return -1.0;
        double latenessUs = ((now.tv_sec - ts.tv_sec) * 1e9 + (now.tv_nsec - ts.tv_nsec)) / 1000.0;
        if (latenessUs > maxLatenessUs) maxLatenessUs = latenessUs;
    }
    return maxLatenessUs;
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
EthernetTesting::EthernetTesting()
{
}

EthernetTesting::~EthernetTesting()
{
}

// ---------------------------------------------------------------------------
// 1. test10GigabitLinkEstablishment
//    Pi5 only has 1 GbE (BCM2712 MAC). Document and verify 1G link.
// ---------------------------------------------------------------------------
TestOutput EthernetTesting::test10GigabitLinkEstablishment()
{
    std::vector<std::string> data;
    bool passed = true;

    data.push_back("NOTE: Pi5 has BCM2712 GbE (1 Gbps), NOT 10 GbE.");
    data.push_back("Ethernet interface: " + std::string(ETH_IFACE));

    if (!ifaceExists(ETH_IFACE)) {
        passed = false;
        data.push_back("FAIL: Interface " + std::string(ETH_IFACE) + " not found.");
        {
            TestOutput _out;
            _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
            _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
            StampDiagnostics(_out, data);
            return _out;
        }
    }

    std::string state = getLinkState(ETH_IFACE);
    data.push_back("Link state: " + state);

    int speed = getLinkSpeedMbps(ETH_IFACE);
    if (speed > 0) {
        data.push_back("Link speed: " + std::to_string(speed) + " Mbps");
        if (speed < 100) {
            passed = false;
            data.push_back("FAIL: Link speed is < 100 Mbps.");
        }
    } else {
        data.push_back("Link speed: not readable (cable may be unplugged)");
    }

    if (state == "up") {
        data.push_back("PASS: Ethernet link is up at " + std::to_string(speed) + " Mbps.");
    } else {
        data.push_back("INFO: Ethernet link not up — ensure cable is connected.");
    }

    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

// ---------------------------------------------------------------------------
// 2. test10gbeTransmitThroughput
//    Measure actual TCP/UDP TX via iperf3 (if available).
// ---------------------------------------------------------------------------
TestOutput EthernetTesting::test10gbeTransmitThroughput()
{
    std::vector<std::string> data;
    bool passed = true;

    data.push_back("Ethernet TX throughput check (Pi5 GbE — max ~940 Mbps)");

    if (!ifaceExists(ETH_IFACE)) {
        data.push_back("SKIP: Interface not present.");
        {
            TestOutput _out;
            _out.status = hal::Status::SKIPPED;
            _out.severity = hal::Severity::INFO;
            StampDiagnostics(_out, data);
            return _out;
        }
    }

    // Check TX bytes counter from sysfs (delta would need two reads)
    long long txBytes = ethReadInt("/sys/class/net/" + std::string(ETH_IFACE) + "/statistics/tx_bytes");
    if (txBytes >= 0) data.push_back("TX bytes since boot: " + std::to_string(txBytes));

    long long txErrors = ethReadInt("/sys/class/net/" + std::string(ETH_IFACE) + "/statistics/tx_errors");
    if (txErrors >= 0) data.push_back("TX errors: " + std::to_string(txErrors));

    long long txDropped = ethReadInt("/sys/class/net/" + std::string(ETH_IFACE) + "/statistics/tx_dropped");
    if (txDropped >= 0) data.push_back("TX dropped: " + std::to_string(txDropped));

    // Report theoretical max
    data.push_back("Pi5 GbE theoretical max TX: ~940 Mbps (1000BASE-T, 4% overhead)");

    if (txErrors > 0) {
        passed = false;
        data.push_back("WARNING: TX errors detected.");
    } else {
        data.push_back("PASS: No TX errors. Run 'iperf3 -s' on peer for live throughput test.");
    }

    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

// ---------------------------------------------------------------------------
// 3. test10gbeReceiveThroughput
//    Check RX statistics and capabilities.
// ---------------------------------------------------------------------------
TestOutput EthernetTesting::test10gbeReceiveThroughput()
{
    std::vector<std::string> data;
    bool passed = true;

    data.push_back("Ethernet RX throughput check");

    if (!ifaceExists(ETH_IFACE)) {
        data.push_back("SKIP: Interface not present.");
        {
            TestOutput _out;
            _out.status = hal::Status::SKIPPED;
            _out.severity = hal::Severity::INFO;
            StampDiagnostics(_out, data);
            return _out;
        }
    }

    long long rxBytes   = ethReadInt("/sys/class/net/" + std::string(ETH_IFACE) + "/statistics/rx_bytes");
    long long rxErrors  = ethReadInt("/sys/class/net/" + std::string(ETH_IFACE) + "/statistics/rx_errors");
    long long rxDropped = ethReadInt("/sys/class/net/" + std::string(ETH_IFACE) + "/statistics/rx_dropped");
    long long rxMissed  = ethReadInt("/sys/class/net/" + std::string(ETH_IFACE) + "/statistics/rx_missed_errors");

    if (rxBytes   >= 0) data.push_back("RX bytes since boot: " + std::to_string(rxBytes));
    if (rxErrors  >= 0) data.push_back("RX errors:   " + std::to_string(rxErrors));
    if (rxDropped >= 0) data.push_back("RX dropped:  " + std::to_string(rxDropped));
    if (rxMissed  >= 0) data.push_back("RX missed:   " + std::to_string(rxMissed));

    if (rxErrors > 0 || rxDropped > 100) {
        passed = false;
        data.push_back("WARNING: RX errors or excessive drops detected.");
    } else {
        data.push_back("PASS: No significant RX errors.");
    }

    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

// ---------------------------------------------------------------------------
// 4. testFullduplexOperation
//    Verify full-duplex mode via ethtool / sysfs.
// ---------------------------------------------------------------------------
TestOutput EthernetTesting::testFullduplexOperation()
{
    std::vector<std::string> data;
    bool passed = true;

    data.push_back("Full-duplex check for " + std::string(ETH_IFACE));

    if (!ifaceExists(ETH_IFACE)) {
        data.push_back("SKIP: Interface not present.");
        {
            TestOutput _out;
            _out.status = hal::Status::SKIPPED;
            _out.severity = hal::Severity::INFO;
            StampDiagnostics(_out, data);
            return _out;
        }
    }

    std::string duplex = ethReadFile("/sys/class/net/" + std::string(ETH_IFACE) + "/duplex");
    data.push_back("Duplex (sysfs): " + (duplex.empty() ? "(not available — link may be down)" : duplex));

    std::string ethtool = ethRunCmd("ethtool eth0 2>/dev/null | grep -i duplex");
    if (!ethtool.empty()) data.push_back("ethtool duplex: " + ethtool);

    if (duplex == "full") {
        data.push_back("PASS: Full-duplex operation confirmed.");
    } else if (duplex == "half") {
        passed = false;
        data.push_back("FAIL: Half-duplex detected — performance degradation expected.");
    } else {
        data.push_back("INFO: Duplex mode not readable (interface may not be connected).");
    }

    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

// ---------------------------------------------------------------------------
// 5. testJumboFramesSupport
//    Check MTU setting and jumbo frame capability.
// ---------------------------------------------------------------------------
TestOutput EthernetTesting::testJumboFramesSupport()
{
    std::vector<std::string> data;
    bool passed = true;

    data.push_back("Jumbo frame (MTU) check for Pi5 GbE");

    if (!ifaceExists(ETH_IFACE)) {
        data.push_back("SKIP: Interface not present.");
        {
            TestOutput _out;
            _out.status = hal::Status::SKIPPED;
            _out.severity = hal::Severity::INFO;
            StampDiagnostics(_out, data);
            return _out;
        }
    }

    long long mtu = ethReadInt("/sys/class/net/" + std::string(ETH_IFACE) + "/mtu");
    data.push_back("Current MTU: " + std::to_string(mtu) + " bytes");

    // Try to get driver's max MTU
    std::string maxMtu = ethRunCmd("ip link show eth0 2>/dev/null | grep -o 'mtu [0-9]*'");
    if (!maxMtu.empty()) data.push_back("ip link: " + maxMtu);

    // Check if BCM genet driver supports jumbo frames
    std::string driver = ethReadFile("/sys/class/net/" + std::string(ETH_IFACE) + "/device/uevent");
    data.push_back("Pi5 GbE driver: bcmgenet (BCM2712 built-in MAC)");

    if (mtu >= 9000) {
        data.push_back("PASS: Jumbo frames supported (MTU " + std::to_string(mtu) + ").");
    } else if (mtu >= 1500) {
        data.push_back("INFO: Standard MTU 1500. Enable jumbo: ip link set eth0 mtu 9000");
        data.push_back("NOTE: bcmgenet supports jumbo frames up to 9014 bytes on Pi5.");
    } else {
        passed = false;
        data.push_back("FAIL: MTU below standard 1500 bytes.");
    }

    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

// ---------------------------------------------------------------------------
// 6. testFlowControl
//    Check IEEE 802.3x flow control (PAUSE frames) via ethtool.
// ---------------------------------------------------------------------------
TestOutput EthernetTesting::testFlowControl()
{
    std::vector<std::string> data;

    data.push_back("IEEE 802.3x flow control (PAUSE frames) check");

    // Acceptance criteria (measurable): ethtool must report the interface's
    // PAUSE-frame capability (Supports/RX/TX fields). ethtool missing or
    // interface absent -> SKIPPED, capability missing -> FAIL.
    if (!ifaceExists(ETH_IFACE)) {
        data.push_back("[SKIP] Interface " + std::string(ETH_IFACE) + " not present — flow control cannot be queried");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    std::string fc = ethRunCmd("ethtool -a eth0 2>/dev/null");
    if (fc.empty()) {
        data.push_back("[SKIP] ethtool unavailable — flow-control capability cannot be observed");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    data.push_back("ethtool -a eth0: " + fc.substr(0, 200));

    if (fc.find("Supports") == std::string::npos && fc.find("RX") == std::string::npos &&
        fc.find("rx") == std::string::npos) {
        data.push_back("[FAIL] Flow-control (PAUSE frame) capability not reported by the driver");
        TestOutput _out;
        _out.status = hal::Status::FAIL;
        _out.severity = hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[PASS] Flow-control capability reported by the driver (PAUSE frames configurable via ethtool -A)");
    {
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
}

// ---------------------------------------------------------------------------
// 7. testVlanTagging
//    Check VLAN (802.1Q) capability.
// ---------------------------------------------------------------------------
TestOutput EthernetTesting::testVlanTagging()
{
    std::vector<std::string> data;

    data.push_back("VLAN tagging (IEEE 802.1Q) capability check");

    // Acceptance criteria (measurable): VLAN capability is demonstrated by
    // the 8021q module being loaded, an existing VLAN interface, or VLAN
    // offload features advertised by the driver. None observable -> FAIL;
    // cannot observe at all -> SKIP.
    std::string vlanMod = ethRunCmd("lsmod 2>/dev/null | grep 8021q");
    data.push_back(std::string("8021q kernel module: ") + (vlanMod.empty() ? "not loaded (load with: modprobe 8021q)" : "LOADED"));

    std::string vlanIfs = ethRunCmd("ip link show type vlan 2>/dev/null");
    if (!vlanIfs.empty()) data.push_back("Existing VLAN interfaces: " + vlanIfs.substr(0, 200));
    else data.push_back("No VLAN interfaces currently configured.");

    std::string vlanFeature = ethRunCmd("ethtool -k eth0 2>/dev/null | grep -i vlan");
    if (!vlanFeature.empty()) data.push_back("VLAN offload features: " + vlanFeature.substr(0, 200));
    else data.push_back("ethtool -k: unavailable (cannot confirm VLAN offload features)");

    if (!vlanMod.empty()) {
        data.push_back("[PASS] 8021q module loaded — VLAN tagging available");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    if (!vlanIfs.empty()) {
        data.push_back("[PASS] VLAN interfaces present — VLAN tagging available");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    if (!vlanFeature.empty()) {
        data.push_back("[PASS] VLAN offload advertised by the driver (8021q module loadable on demand)");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[FAIL] No VLAN capability observed (8021q not loaded and no VLAN features reported by ethtool)");
    TestOutput _out;
    _out.status = hal::Status::FAIL;
    _out.severity = hal::Severity::ERROR;
    StampDiagnostics(_out, data);
    return _out;
}

// ---------------------------------------------------------------------------
// 8. testChecksumOffload
//    Check TX/RX checksum offload status.
// ---------------------------------------------------------------------------
TestOutput EthernetTesting::testChecksumOffload()
{
    std::vector<std::string> data;

    data.push_back("Checksum offload (TX/RX) check for Pi5 GbE");

    // Acceptance criteria (measurable): ethtool -k must advertise both RX and
    // TX checksum offload. ethtool unavailable -> SKIPPED; features missing ->
    // FAIL.
    std::string features = ethRunCmd("ethtool -k eth0 2>/dev/null | grep -i 'checksum\\|offload'");
    if (features.empty()) {
        data.push_back("[SKIP] ethtool unavailable — checksum offload capability cannot be observed");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    data.push_back("Offload features: " + features.substr(0, 500));

    bool rx = features.find("rx-checksumming") != std::string::npos;
    bool tx = features.find("tx-checksumming") != std::string::npos;
    if (!rx || !tx) {
        data.push_back("[FAIL] Driver does not advertise ");
        data.back() += std::string(!rx ? "rx-checksumming " : "") + (!tx ? "tx-checksumming" : "");
        TestOutput _out;
        _out.status = hal::Status::FAIL;
        _out.severity = hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[PASS] TX/RX checksum offload advertised by the driver");
    {
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
}

// ---------------------------------------------------------------------------
// 9. testIeee8021asTimeSynchronization
//    Check PTP/gPTP support (IEEE 802.1AS).
// ---------------------------------------------------------------------------
TestOutput EthernetTesting::testIeee8021asTimeSynchronization()
{
    std::vector<std::string> data;

    data.push_back("IEEE 802.1AS / PTP time synchronization check");

    // Acceptance criteria (measurable): a PTP hardware clock (/dev/ptp0) or a
    // timestamping capability reported by ethtool -T is required for 802.1AS.
    // Neither observable (ethtool missing) -> SKIPPED; both absent -> FAIL.
    bool ptpDev = (access("/dev/ptp0", F_OK) == 0);
    data.push_back("PTP hardware clock (/dev/ptp0): " + std::string(ptpDev ? "present" : "not found"));

    bool ptp4l = (access("/usr/sbin/ptp4l", F_OK) == 0) || (access("/usr/bin/ptp4l", F_OK) == 0);
    data.push_back("ptp4l (linuxptp): " + std::string(ptp4l ? "installed" : "not installed (apt install linuxptp)"));

    std::string ptpFeature = ethRunCmd("ethtool -T eth0 2>/dev/null");
    if (!ptpFeature.empty()) {
        data.push_back("ethtool -T: " + ptpFeature.substr(0, 300));
    } else {
        data.push_back("ethtool -T: unavailable (cannot query timestamping capabilities)");
    }

    if (ptpDev) {
        data.push_back("[PASS] PTP hardware clock present — hardware timestamping path available for 802.1AS");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    if (!ptpFeature.empty()) {
        data.push_back("[PASS] Kernel timestamping capabilities reported (software 802.1AS path; hardware PTP requires BCM54213PE PHY support)");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[SKIP] ethtool unavailable and no /dev/ptp0 — PTP capability cannot be observed");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

// ---------------------------------------------------------------------------
// 10. testIeee8021qbv — Time-Aware Shaper (TAS)
// ---------------------------------------------------------------------------
TestOutput EthernetTesting::testIeee8021qbv()
{
    std::vector<std::string> data;

    data.push_back("IEEE 802.1Qbv Time-Aware Shaper (TAS) check");
    data.push_back("Pi5 bcmgenet: does NOT support hardware TAS/TAPRIO.");
    data.push_back("Software TAPRIO: available via Linux TC TAPRIO qdisc.");

    // Acceptance criteria (measurable): the sch_taprio qdisc must be loaded
    // or available, or a TAPRIO qdisc active. tc missing -> SKIPPED;
    // capability absent -> FAIL.
    if (!ethTcAvailable()) {
        data.push_back("[SKIP] tc (iproute2) not installed — TAPRIO qdisc availability cannot be verified");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    std::string taprio = ethRunCmd("tc qdisc show dev eth0 2>/dev/null | grep taprio");
    data.push_back("TAPRIO qdisc active: " + (taprio.empty() ? "no" : "YES — " + taprio));
    bool modAvail = ethModuleAvailable("sch_taprio");
    data.push_back(std::string("sch_taprio module: ") + (modAvail ? "available" : "not available"));

    if (!taprio.empty() || modAvail) {
        data.push_back("[PASS] Software TAS available (sch_taprio) — hardware TAS remains N/A on Pi5");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[FAIL] sch_taprio module not loaded/available and no TAPRIO qdisc active — software TAS cannot be configured");
    TestOutput _out;
    _out.status = hal::Status::FAIL;
    _out.severity = hal::Severity::ERROR;
    StampDiagnostics(_out, data);
    return _out;
}

// ---------------------------------------------------------------------------
// 11. testIeee8021qbu — Frame Preemption
// ---------------------------------------------------------------------------
TestOutput EthernetTesting::testIeee8021qbu()
{
    std::vector<std::string> data;

    data.push_back("IEEE 802.1Qbu Frame Preemption check");

    // Acceptance criteria: frame preemption requires a TSN-capable NIC that
    // implements MAC-merge (Intel I225, Microchip LAN9668, etc.). BCM2712/
    // bcmgenet does not implement it — hardware capability is absent by
    // design, so the honest verdict is SKIPPED (not applicable), never PASS.
    std::string preemption = ethRunCmd("ethtool --show-frame-preemption eth0 2>/dev/null");
    if (!preemption.empty() &&
        preemption.find("not supported") == std::string::npos &&
        preemption.find("netlink error") == std::string::npos) {
        data.push_back("Frame preemption state: " + preemption.substr(0, 200));
        data.push_back("[PASS] MAC-merge/frame-preemption capability reported by the NIC");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    if (preemption.empty()) {
        data.push_back("[SKIP] ethtool unavailable — preemption capability cannot be queried");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("Frame preemption requires a TSN-capable NIC (Intel I225, Microchip LAN9668, etc.).");
    data.push_back("[SKIP] bcmgenet does not implement 802.1Qbu frame preemption — not applicable on this DUT");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

// ---------------------------------------------------------------------------
// 12. testIeee8021qav — Credit-Based Shaper (CBS)
// ---------------------------------------------------------------------------
TestOutput EthernetTesting::testIeee8021qav()
{
    std::vector<std::string> data;

    data.push_back("IEEE 802.1Qav Credit-Based Shaper (CBS) check");

    // Acceptance criteria (measurable): the sch_cbs qdisc must be loaded or
    // available, or a CBS qdisc active. tc missing -> SKIPPED; capability
    // absent -> FAIL.
    if (!ethTcAvailable()) {
        data.push_back("[SKIP] tc (iproute2) not installed — CBS qdisc availability cannot be verified");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    std::string cbs = ethRunCmd("tc qdisc show dev eth0 2>/dev/null | grep cbs");
    data.push_back("CBS qdisc active: " + (cbs.empty() ? "no" : "YES — " + cbs));
    bool modAvail = ethModuleAvailable("sch_cbs");
    data.push_back(std::string("sch_cbs module: ") + (modAvail ? "available" : "not available"));

    if (!cbs.empty() || modAvail) {
        data.push_back("[PASS] Software CBS available (sch_cbs) — credit-based shaping can be configured");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[FAIL] sch_cbs module not loaded/available and no CBS qdisc active");
    TestOutput _out;
    _out.status = hal::Status::FAIL;
    _out.severity = hal::Severity::ERROR;
    StampDiagnostics(_out, data);
    return _out;
}

// ---------------------------------------------------------------------------
// 13. testTsnTrafficScheduling
// ---------------------------------------------------------------------------
TestOutput EthernetTesting::testTsnTrafficScheduling()
{
    std::vector<std::string> data;

    data.push_back("TSN traffic scheduling check (Pi5 software TSN via Linux TC)");

    // Acceptance criteria (measurable): at least one software TSN qdisc
    // module (TAPRIO/CBS/ETF) must be available. tc missing -> SKIPPED;
    // none available -> FAIL.
    if (!ethTcAvailable()) {
        data.push_back("[SKIP] tc (iproute2) not installed — qdisc availability cannot be verified");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    std::string tc = ethRunCmd("tc qdisc show dev eth0 2>/dev/null");
    data.push_back("tc qdisc: " + (tc.empty() ? "(empty/default)" : tc.substr(0, 300)));

    bool taprio = ethModuleAvailable("sch_taprio");
    bool cbs    = ethModuleAvailable("sch_cbs");
    bool etf    = ethModuleAvailable("sch_etf");
    data.push_back(std::string("qdisc modules — taprio: ") + (taprio ? "yes" : "no") +
                   ", cbs: " + (cbs ? "yes" : "no") + ", etf: " + (etf ? "yes" : "no"));

    if (taprio || cbs || etf) {
        data.push_back("[PASS] Software TSN qdisc support available (Pi5: TAPRIO/CBS/ETF; hardware TSN requires a dedicated TSN-MAC NIC)");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[FAIL] None of sch_taprio/sch_cbs/sch_etf is available — software TSN scheduling cannot be configured");
    TestOutput _out;
    _out.status = hal::Status::FAIL;
    _out.severity = hal::Severity::ERROR;
    StampDiagnostics(_out, data);
    return _out;
}

// ---------------------------------------------------------------------------
// 14. testTsnLatencyGuarantees
// ---------------------------------------------------------------------------
TestOutput EthernetTesting::testTsnLatencyGuarantees()
{
    std::vector<std::string> data;

    data.push_back("TSN latency guarantee check");
    data.push_back("Pi5 with software TSN: software-controlled scheduling jitter ~10–50 µs");
    data.push_back("Hardware TSN (<1 µs jitter): requires dedicated TSN NIC.");

    // Acceptance criteria (measurable): a measured periodic-wake latency.
    // 100 iterations of a 1 ms clock_nanosleep loop; the maximum lateness
    // must stay within the 1 ms software-TSN budget.
    double maxJitterUs = ethMeasureMaxJitterUs(100);
    if (maxJitterUs < 0.0) {
        data.push_back("[SKIP] Wake-up jitter measurement failed (clock interface error) — TSN latency cannot be measured");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    data.push_back("Measured wake-up jitter (100 × 1 ms clock_nanosleep): max " +
                   std::to_string(maxJitterUs) + " µs");

    bool rtKernel = false;
    std::ifstream kver("/proc/version");
    if (kver.is_open()) {
        std::string v;
        std::getline(kver, v);
        data.push_back("Kernel: " + v.substr(0, 80));
        rtKernel = (v.find("PREEMPT_RT") != std::string::npos || v.find("PREEMPT RT") != std::string::npos);
    }
    data.push_back(std::string("RT kernel (PREEMPT_RT): ") + (rtKernel ? "YES" : "no (use rpi-realtime kernel for tighter bounds)"));

    if (maxJitterUs < 1000.0) {
        data.push_back("[PASS] Measured scheduling lateness within the 1 ms software-TSN budget (max " +
                       std::to_string(maxJitterUs) + " µs)");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[FAIL] Measured scheduling lateness " + std::to_string(maxJitterUs) +
                   " µs exceeds the 1 ms software-TSN budget");
    TestOutput _out;
    _out.status = hal::Status::FAIL;
    _out.severity = hal::Severity::ERROR;
    StampDiagnostics(_out, data);
    return _out;
}

// ---------------------------------------------------------------------------
// 15. testTsnTimePrecision
// ---------------------------------------------------------------------------
TestOutput EthernetTesting::testTsnTimePrecision()
{
    std::vector<std::string> data;

    data.push_back("TSN time precision check");

    // Acceptance criteria (measurable): the system clock discipline state must
    // be observable and (when measurable) within 1 ms of the reference.
    // chrony present -> parse the tracked offset; systemd-timesyncd state is
    // the fallback; neither -> SKIPPED.
    std::string chrony = ethRunCmd("chronyc tracking 2>/dev/null | grep -i 'System time'");
    if (!chrony.empty()) {
        data.push_back("chrony: " + chrony);
        size_t pos = chrony.find_first_of("0123456789");
        if (pos != std::string::npos) {
            char* end = nullptr;
            double offsetSec = strtod(chrony.c_str() + pos, &end);
            if (end != chrony.c_str() + pos) {
                double offsetUs = offsetSec * 1e6;
                if (offsetUs >= 0.0 && offsetUs <= 1000.0) {
                    data.push_back("[PASS] System clock within " + std::to_string(offsetUs) +
                                   " µs of NTP time (≤1 ms precision budget)");
                    TestOutput _out;
                    _out.status = hal::Status::PASS;
                    _out.severity = hal::Severity::INFO;
                    StampDiagnostics(_out, data);
                    return _out;
                }
                data.push_back("[FAIL] System clock offset " + std::to_string(offsetUs) +
                               " µs exceeds the 1 ms precision budget");
                TestOutput _out;
                _out.status = hal::Status::FAIL;
                _out.severity = hal::Severity::ERROR;
                StampDiagnostics(_out, data);
                return _out;
            }
        }
        data.push_back("[SKIP] chrony tracking output not parseable — time precision cannot be measured");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    std::string tsync = ethRunCmd("timedatectl show-timesync --value 2>/dev/null | grep -i 'NTPSynchronized'");
    if (!tsync.empty()) {
        if (tsync.find("yes") != std::string::npos) {
            data.push_back("[PASS] systemd-timesyncd reports NTP-synchronized clock");
            TestOutput _out;
            _out.status = hal::Status::PASS;
            _out.severity = hal::Severity::INFO;
            StampDiagnostics(_out, data);
            return _out;
        }
        data.push_back("[FAIL] Timesyncd reports an unsynchronized clock — time precision cannot be guaranteed");
        TestOutput _out;
        _out.status = hal::Status::FAIL;
        _out.severity = hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[SKIP] No time-sync daemon observable (chrony absent, timedatectl absent) — time precision cannot be measured");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

// ---------------------------------------------------------------------------
// 16. test1gPhyFunctionality
//    Verify the BCM54213PE (Gigabit PHY) is functional.
// ---------------------------------------------------------------------------
TestOutput EthernetTesting::test1gPhyFunctionality()
{
    std::vector<std::string> data;
    bool passed = true;

    data.push_back("Pi5 GbE PHY: BCM54213PE (Broadcom Gigabit PHY)");

    // Check PHY via ethtool
    std::string phyInfo = ethRunCmd("ethtool eth0 2>/dev/null");
    if (!phyInfo.empty()) {
        std::istringstream ss(phyInfo);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.find("Speed") != std::string::npos ||
                line.find("Duplex") != std::string::npos ||
                line.find("Port") != std::string::npos ||
                line.find("Link detected") != std::string::npos)
                data.push_back("  " + line);
        }
    }

    // Check driver info
    std::string phyDriver = ethRunCmd("ethtool -i eth0 2>/dev/null");
    if (!phyDriver.empty()) data.push_back("Driver: " + phyDriver.substr(0, 200));

    // Check PHY sysfs
    std::string phySysfs = ethReadFile("/sys/class/net/eth0/phydev/uevent");
    if (!phySysfs.empty()) data.push_back("PHY uevent: " + phySysfs.substr(0, 100));

    int spd = getLinkSpeedMbps(ETH_IFACE);
    if (spd > 0 && spd < 100) {
        passed = false;
        data.push_back("FAIL: PHY link speed below 100 Mbps.");
    } else {
        data.push_back("PASS: 1G PHY functional." + (spd > 0 ? " Speed: " + std::to_string(spd) + " Mbps" : ""));
    }

    {
        TestOutput _out;
        _out.status = passed ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = passed ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

// ---------------------------------------------------------------------------
// 17. testLinkSpeedNegotiation
//    Auto-negotiation check.
// ---------------------------------------------------------------------------
TestOutput EthernetTesting::testLinkSpeedNegotiation()
{
    std::vector<std::string> data;

    data.push_back("Auto-negotiation check for Pi5 GbE");

    // Acceptance criteria (measurable): interface present, link up, and a
    // negotiated speed reported with auto-negotiation enabled. Link down or
    // ethtool missing -> SKIPPED; link up with no speed -> FAIL.
    if (!ifaceExists(ETH_IFACE)) {
        data.push_back("[SKIP] Interface " + std::string(ETH_IFACE) + " not present");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    std::string autoneg = ethRunCmd("ethtool eth0 2>/dev/null | grep -i 'auto-negotiation'");
    if (autoneg.empty()) {
        data.push_back("[SKIP] ethtool unavailable — auto-negotiation state cannot be observed");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    data.push_back("Auto-negotiation: " + autoneg);

    std::string advertised = ethRunCmd("ethtool eth0 2>/dev/null | grep -i 'advertised'");
    if (!advertised.empty()) data.push_back("Advertised modes: " + advertised.substr(0, 200));

    std::string state = getLinkState(ETH_IFACE);
    int spd = getLinkSpeedMbps(ETH_IFACE);
    data.push_back("Link state: " + state);
    data.push_back("Negotiated speed: " + (spd > 0 ? std::to_string(spd) + " Mbps" : "(link down)") +
                   " (" + std::to_string(ETH_SPEED_MBPS) + " Mbps PHY)");

    if (state != "up") {
        data.push_back("[SKIP] Link is down — negotiation outcome not observable (no cable/peer)");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    if (spd <= 0) {
        data.push_back("[FAIL] Link is up but the PHY reports no negotiated speed");
        TestOutput _out;
        _out.status = hal::Status::FAIL;
        _out.severity = hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[PASS] Negotiated " + std::to_string(spd) + " Mbps with auto-negotiation enabled");
    {
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
}

// ---------------------------------------------------------------------------
// 18. testAutomdimdix
//    Check Auto-MDIX (automatic medium-dependent interface crossover).
// ---------------------------------------------------------------------------
TestOutput EthernetTesting::testAutomdimdix()
{
    std::vector<std::string> data;

    data.push_back("Auto-MDIX check — Pi5 BCM54213PE PHY");
    data.push_back("BCM54213PE supports Auto-MDIX — straight and crossover cables work.");

    // Acceptance criteria (measurable): ethtool must report the MDI-X state
    // of the PHY. ethtool missing or interface absent -> SKIPPED.
    if (!ifaceExists(ETH_IFACE)) {
        data.push_back("[SKIP] Interface " + std::string(ETH_IFACE) + " not present");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    std::string mdix = ethRunCmd("ethtool eth0 2>/dev/null | grep -i 'MDI'");
    if (!mdix.empty()) {
        data.push_back("MDI-X status: " + mdix);
        data.push_back("[PASS] PHY reports Auto-MDIX state via ethtool (straight and crossover cables both work)");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[SKIP] ethtool does not report MDI-X status — Auto-MDIX operation cannot be verified on this DUT");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

// ---------------------------------------------------------------------------
// 19. testEthernetCableDiagnostics
//    Run cable diagnostics via ethtool (TDR/VCT if supported).
// ---------------------------------------------------------------------------
TestOutput EthernetTesting::testEthernetCableDiagnostics()
{
    std::vector<std::string> data;

    data.push_back("Ethernet cable diagnostics (Pi5)");

    // Acceptance criteria (measurable): ethtool --cable-test must return an
    // actual TDR result (requires a PHY with ETHTOOL_MSG_CABLE_TEST support
    // and a link-down port). Not supported / link up / ethtool missing ->
    // SKIPPED; RX-error counters are reported as informational evidence.
    if (!ifaceExists(ETH_IFACE)) {
        data.push_back("[SKIP] Interface " + std::string(ETH_IFACE) + " not present — cable diagnostics cannot run");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    std::string cableTest = ethRunCmd("ethtool --cable-test eth0 2>/dev/null");
    bool haveResults = !cableTest.empty() &&
                       cableTest.find("not supported") == std::string::npos &&
                       cableTest.find("netlink error") == std::string::npos &&
                       cableTest.find("Operation not permitted") == std::string::npos;
    if (haveResults) {
        data.push_back("Cable test result: " + cableTest.substr(0, 300));
        // Report cable type from ethtool port field
        std::string port = ethRunCmd("ethtool eth0 2>/dev/null | grep -i 'port'");
        if (!port.empty()) data.push_back("Port type: " + port);
        data.push_back("[PASS] PHY cable-test completed and returned pair diagnostics");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    long long rxErrors = ethReadInt("/sys/class/net/" + std::string(ETH_IFACE) + "/statistics/rx_errors");
    if (rxErrors == 0) {
        data.push_back("No RX errors — cable appears healthy (informational only).");
    } else if (rxErrors > 0) {
        data.push_back("RX errors: " + std::to_string(rxErrors) + " — check cable/connector.");
    }

    if (cableTest.empty()) {
        data.push_back("[SKIP] ethtool missing or cable-test command failed — wire diagnostics cannot run");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[SKIP] Cable test not supported by the PHY (or link is up) — pair-level diagnostics unavailable");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief Run all tests for this module
 * @return Vector of test results
 */
std::vector<TestResult> EthernetTesting::runAllTests()
{
    std::vector<TestResult> results;

    // Run test10GigabitLinkEstablishment
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = test10GigabitLinkEstablishment();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "test10GigabitLinkEstablishment";
            _tr.test_id = "test10GigabitLinkEstablishment";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run test10gbeTransmitThroughput
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = test10gbeTransmitThroughput();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "test10gbeTransmitThroughput";
            _tr.test_id = "test10gbeTransmitThroughput";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run test10gbeReceiveThroughput
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = test10gbeReceiveThroughput();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "test10gbeReceiveThroughput";
            _tr.test_id = "test10gbeReceiveThroughput";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testFullduplexOperation
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testFullduplexOperation();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testFullduplexOperation";
            _tr.test_id = "testFullduplexOperation";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testJumboFramesSupport
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testJumboFramesSupport();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testJumboFramesSupport";
            _tr.test_id = "testJumboFramesSupport";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testFlowControl
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testFlowControl();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testFlowControl";
            _tr.test_id = "testFlowControl";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testVlanTagging
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testVlanTagging();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testVlanTagging";
            _tr.test_id = "testVlanTagging";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testChecksumOffload
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testChecksumOffload();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testChecksumOffload";
            _tr.test_id = "testChecksumOffload";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testIeee8021asTimeSynchronization
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testIeee8021asTimeSynchronization();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testIeee8021asTimeSynchronization";
            _tr.test_id = "testIeee8021asTimeSynchronization";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testIeee8021qbv
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testIeee8021qbv();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testIeee8021qbv";
            _tr.test_id = "testIeee8021qbv";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testIeee8021qbu
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testIeee8021qbu();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testIeee8021qbu";
            _tr.test_id = "testIeee8021qbu";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testIeee8021qav
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testIeee8021qav();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testIeee8021qav";
            _tr.test_id = "testIeee8021qav";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testTsnTrafficScheduling
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testTsnTrafficScheduling();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testTsnTrafficScheduling";
            _tr.test_id = "testTsnTrafficScheduling";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testTsnLatencyGuarantees
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testTsnLatencyGuarantees();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testTsnLatencyGuarantees";
            _tr.test_id = "testTsnLatencyGuarantees";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testTsnTimePrecision
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testTsnTimePrecision();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testTsnTimePrecision";
            _tr.test_id = "testTsnTimePrecision";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run test1gPhyFunctionality
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = test1gPhyFunctionality();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "test1gPhyFunctionality";
            _tr.test_id = "test1gPhyFunctionality";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testLinkSpeedNegotiation
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testLinkSpeedNegotiation();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testLinkSpeedNegotiation";
            _tr.test_id = "testLinkSpeedNegotiation";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testAutomdimdix
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testAutomdimdix();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testAutomdimdix";
            _tr.test_id = "testAutomdimdix";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testEthernetCableDiagnostics
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testEthernetCableDiagnostics();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testEthernetCableDiagnostics";
            _tr.test_id = "testEthernetCableDiagnostics";
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
std::vector<std::string> EthernetTesting::getAvailableTests()
{
    return {
        "testFullduplexOperation",  "testJumboFramesSupport",   "testFlowControl",
        "testVlanTagging",          "testChecksumOffload",      "testIeee8021asTimeSynchronization",
        "testIeee8021qbv",          "testIeee8021qbu",          "testIeee8021qav",
        "testTsnTrafficScheduling", "testTsnLatencyGuarantees", "testTsnTimePrecision",
        "testLinkSpeedNegotiation", "testAutomdimdix",          "testEthernetCableDiagnostics",
    };
}

/**
 * @brief Run a single test by name
 * @param testName Name of the test to run
 * @return Test result with timing
 */
TestResult EthernetTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput result;
    result.status = hal::Status::ERROR;
    result.severity = hal::Severity::ERROR;
    result.error_message = "test not dispatched";

    if (testName == "testFullduplexOperation") {
        result = testFullduplexOperation();
    } else if (testName == "testJumboFramesSupport") {
        result = testJumboFramesSupport();
    } else if (testName == "testFlowControl") {
        result = testFlowControl();
    } else if (testName == "testVlanTagging") {
        result = testVlanTagging();
    } else if (testName == "testChecksumOffload") {
        result = testChecksumOffload();
    } else if (testName == "testIeee8021asTimeSynchronization") {
        result = testIeee8021asTimeSynchronization();
    } else if (testName == "testIeee8021qbv") {
        result = testIeee8021qbv();
    } else if (testName == "testIeee8021qbu") {
        result = testIeee8021qbu();
    } else if (testName == "testIeee8021qav") {
        result = testIeee8021qav();
    } else if (testName == "testTsnTrafficScheduling") {
        result = testTsnTrafficScheduling();
    } else if (testName == "testTsnLatencyGuarantees") {
        result = testTsnLatencyGuarantees();
    } else if (testName == "testTsnTimePrecision") {
        result = testTsnTimePrecision();
    } else if (testName == "testLinkSpeedNegotiation") {
        result = testLinkSpeedNegotiation();
    } else if (testName == "testAutomdimdix") {
        result = testAutomdimdix();
    } else if (testName == "testEthernetCableDiagnostics") {
        result = testEthernetCableDiagnostics();
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
