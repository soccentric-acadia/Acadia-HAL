/**
 * @file display_interfaces.cpp
 * @brief Implementation file for DisplayInterfaces class — Raspberry Pi 5
 *        Pi5 has 2x micro-HDMI ports (HDMI0/HDMI1), no DisplayPort.
 *        Supports 4K@60Hz on HDMI0, 4K@60Hz on HDMI1 simultaneously.
 * @author sandesh ghimire
 * @copyright (C) Soccentric LLC */

#include "display_interfaces.h"
#include "../platform_rpi5.h"

#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::string dispReadFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open())
        return "";
    std::string s;
    std::getline(f, s);
    return s;
}

static std::string dispRunCmd(const char* cmd)
{
    char buf[8192] = {};
    FILE* pipe = popen(cmd, "r");
    if (!pipe)
        return "";
    size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
    pclose(pipe);
    buf[n] = '\0';
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
        buf[--n] = '\0';
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// DRM connector helpers (Pi5: micro-HDMI connectors are HDMI-A-1 / HDMI-A-2)
// ---------------------------------------------------------------------------
static std::string dispHdmiConnectorBase(const char* hdmi)
{
    std::string base = "/sys/class/drm/card1-" + std::string(hdmi);
    if (access(base.c_str(), F_OK) == 0)
        return base;
    base = "/sys/class/drm/card0-" + std::string(hdmi);
    if (access(base.c_str(), F_OK) == 0)
        return base;
    return "";
}

static int dispHdmiConnectorCount()
{
    static const char* const ports[] = {"HDMI-A-1", "HDMI-A-2"};
    int n = 0;
    for (const char* p : ports)
        if (!dispHdmiConnectorBase(p).empty())
            ++n;
    return n;
}

static bool dispDrmAvailable()
{
    return access("/sys/class/drm", F_OK) == 0;
}

// Parse EDID detailed timing descriptors (base block + CEA-861 extension
// blocks) and collect the vertical refresh rate of each valid progressive
// timing. Returns the number of rates in the sane 23.9..121 Hz range.
static int dispEdidRefreshRates(const std::string& path, std::vector<double>& rates)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        return 0;
    std::string edid((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (edid.size() < 128)
        return 0;

    int count = 0;
    auto addDtd = [&](const unsigned char* d) {
        unsigned pixelClock10kHz = (unsigned)d[0] | ((unsigned)d[1] << 8);
        if (pixelClock10kHz == 0)
            return; // monitor descriptor, not a timing
        unsigned hActive = d[2] | ((d[4] & 0xF0u) << 4);
        unsigned hBlank = d[3] | ((d[4] & 0x0Fu) << 8);
        unsigned vActive = d[5] | ((d[7] & 0xF0u) << 4);
        unsigned vBlank = d[6] | ((d[7] & 0x0Fu) << 8);
        unsigned hTotal = hActive + hBlank;
        unsigned vTotal = vActive + vBlank;
        if (hTotal == 0 || vTotal == 0)
            return;
        double hz = (double)pixelClock10kHz * 10000.0 / ((double)hTotal * (double)vTotal);
        if (hz >= 23.9 && hz <= 121.0) {
            rates.push_back(hz);
            ++count;
        }
    };

    // Base block: up to 4 detailed timing descriptors at offsets 54/72/90/108.
    for (int i = 0; i < 4; ++i) {
        size_t off = 54 + (size_t)i * 18;
        if (off + 18 > edid.size())
            break;
        addDtd((const unsigned char*)edid.data() + off);
    }

    // CEA-861 extension blocks may carry additional detailed timings.
    unsigned nExt = (unsigned char)edid[126];
    for (unsigned e = 0; e < nExt; ++e) {
        size_t baseOff = 128 + 128 * (size_t)e;
        if (baseOff + 128 > edid.size())
            break;
        const unsigned char* ext = (const unsigned char*)edid.data() + baseOff;
        if (ext[0] != 0x02)
            continue; // not a CEA extension
        unsigned dtdStart = ext[2];
        if (dtdStart < 4 || dtdStart > 127)
            continue;
        for (size_t off = baseOff + dtdStart; off + 18 <= baseOff + 128; off += 18)
            addDtd((const unsigned char*)edid.data() + off);
    }
    return count;
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
DisplayInterfaces::DisplayInterfaces()
{
}

DisplayInterfaces::~DisplayInterfaces()
{
}

// ---------------------------------------------------------------------------
// 1. testDisplayPortOutput
//    Pi5 does NOT have native DisplayPort — only micro-HDMI.
// ---------------------------------------------------------------------------
TestOutput DisplayInterfaces::testDisplayPortOutput()
{
    std::vector<std::string> data;

    data.push_back("DisplayPort output check (Pi5)");
    data.push_back("NOTE: Pi5 does NOT have native DisplayPort — 2x micro-HDMI only.");
    data.push_back("DisplayPort-over-USB-C is NOT available on Pi5 (no USB-C DP Alt Mode).");
    data.push_back("USB-C port on Pi5 is power-input only (USB-C PD).");

    // Acceptance criteria (measurable): a DisplayPort sink must exist to test
    // DP output. On Pi5 the native DP path does not exist; the only route is a
    // USB DisplayPort adapter. With no DP sink enumerated there is nothing to
    // verify -> SKIPPED (not applicable), never a blind PASS.
    bool usbDp = !dispRunCmd("lsusb 2>/dev/null | grep -i 'displayport\\|dp adapter'").empty();
    bool drmDp = !dispRunCmd("for f in /sys/class/drm/*DP*; do [ -e \"$f\" ] && echo \"$f\"; done 2>/dev/null").empty();
    if (usbDp)
        data.push_back("USB DisplayPort adapter detected");
    if (drmDp)
        data.push_back("DRM DisplayPort/USB-C connector detected");

    if (!usbDp && !drmDp) {
        data.push_back("[SKIP] No DisplayPort sink available on this DUT (Pi5 exposes micro-HDMI only) — nothing to test");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[PASS] DisplayPort sink enumerated (native DP not supported on Pi5; adapter path present)");
    {
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
}

// ---------------------------------------------------------------------------
// 2. testHdmiOutput
//    Check micro-HDMI 0 and 1 via DRM sysfs.
// ---------------------------------------------------------------------------
TestOutput DisplayInterfaces::testHdmiOutput()
{
    std::vector<std::string> data;

    data.push_back("Pi5 micro-HDMI output check (HDMI0 and HDMI1)");

    if (!dispDrmAvailable()) {
        data.push_back("[SKIP] DRM sysfs not available — KMS driver not loaded, HDMI output cannot be observed");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    std::string drm = dispRunCmd("ls /sys/class/drm/ 2>/dev/null");
    if (!drm.empty())
        data.push_back("DRM devices: " + drm);

    // Acceptance criteria (measurable): at least one micro-HDMI connector
    // node must exist and report an asserted hot-plug (a connected sink),
    // i.e. an output path a display could actually be driven through.
    static const char* const hdmiPorts[] = {"HDMI-A-1", "HDMI-A-2"};
    int present = 0, connected = 0;
    for (const char* hdmi : hdmiPorts) {
        std::string base = dispHdmiConnectorBase(hdmi);
        if (base.empty()) {
            data.push_back(std::string(hdmi) + ": connector node not found");
            continue;
        }
        ++present;
        std::string status = dispReadFile(base + "/status");
        std::string edid = dispReadFile(base + "/edid");
        data.push_back(std::string(hdmi) + ": " + (status.empty() ? "status unreadable" : status) +
                       ", EDID available: " + (edid.empty() ? "no" : "yes"));
        if (status == "connected")
            ++connected;
    }

    if (present == 0) {
        data.push_back("[FAIL] No HDMI connector nodes found in DRM (Pi5 must expose HDMI-A-1/HDMI-A-2)");
        TestOutput _out;
        _out.status = hal::Status::FAIL;
        _out.severity = hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
    if (connected == 0) {
        data.push_back("[SKIP] HDMI controller(s) present but no sink connected (HPD not asserted) — connect a display to verify output");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    // vcgencmd display_power is informational evidence when available.
    std::string dispPwr = dispRunCmd("vcgencmd display_power 2>/dev/null");
    if (!dispPwr.empty())
        data.push_back("vcgencmd display_power: " + dispPwr);

    data.push_back("[PASS] " + std::to_string(connected) + " of " + std::to_string(present) +
                   " micro-HDMI connector(s) present with a connected sink (HPD asserted, EDID path available)");
    {
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
}

// ---------------------------------------------------------------------------
// 3. testMultiDisplaySupport
// ---------------------------------------------------------------------------
TestOutput DisplayInterfaces::testMultiDisplaySupport()
{
    std::vector<std::string> data;

    data.push_back("Multi-display support check (Pi5: 2x micro-HDMI, up to dual 4K@60Hz)");

    if (!dispDrmAvailable()) {
        data.push_back("[SKIP] DRM sysfs not available — cannot enumerate display connectors");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    // Acceptance criteria (measurable): dual-display support requires BOTH
    // micro-HDMI connector nodes to exist in DRM. Missing connector(s) = the
    // controller cannot drive two displays = FAIL.
    int connectors = dispHdmiConnectorCount();
    data.push_back("HDMI connector nodes present: " + std::to_string(connectors) + " / 2");

    std::string crtcs = dispRunCmd("cat /sys/class/drm/card*/crtc_count 2>/dev/null");
    data.push_back("DRM CRTC count: " + (crtcs.empty() ? "not available" : crtcs));

    std::string connectorsList = dispRunCmd("ls /sys/class/drm/ 2>/dev/null | grep -v render");
    if (!connectorsList.empty())
        data.push_back("DRM connectors: " + connectorsList.substr(0, 400));

    if (connectors < 2) {
        data.push_back("[FAIL] Only " + std::to_string(connectors) +
                       " of 2 expected micro-HDMI connector(s) present — dual-display support cannot be verified");
        TestOutput _out;
        _out.status = hal::Status::FAIL;
        _out.severity = hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[PASS] Both micro-HDMI connectors enumerated — dual simultaneous output path available");
    {
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
}

// ---------------------------------------------------------------------------
// 4. testVariousResolutions
// ---------------------------------------------------------------------------
TestOutput DisplayInterfaces::testVariousResolutions()
{
    std::vector<std::string> data;

    data.push_back("Supported resolutions check (Pi5 HDMI via VideoCore VII)");

    if (!dispDrmAvailable()) {
        data.push_back("[SKIP] DRM sysfs not available — cannot enumerate display modes");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    // Acceptance criteria (measurable): the connected sink's EDID/DRM mode
    // list must yield at least one supported resolution. Without a sink this
    // is not observable -> SKIPPED.
    static const char* const ports[] = {"HDMI-A-1", "HDMI-A-2"};
    std::vector<std::string> allModes;
    bool connected = false;
    for (const char* hdmi : ports) {
        std::string base = dispHdmiConnectorBase(hdmi);
        if (base.empty())
            continue;
        if (dispReadFile(base + "/status") == "connected")
            connected = true;
        std::ifstream mf(base + "/modes");
        std::string line;
        while (std::getline(mf, line)) {
            if (!line.empty() && line.find('x') != std::string::npos)
                allModes.push_back(line);
        }
    }

    if (allModes.empty()) {
        if (!connected) {
            data.push_back("[SKIP] No display connected (no EDID/mode list) — connect a display to enumerate supported resolutions");
            TestOutput _out;
            _out.status = hal::Status::SKIPPED;
            _out.severity = hal::Severity::INFO;
            StampDiagnostics(_out, data);
            return _out;
        }
        data.push_back("[FAIL] Display connected but the connector exposes no display modes");
        TestOutput _out;
        _out.status = hal::Status::FAIL;
        _out.severity = hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[PASS] " + std::to_string(allModes.size()) + " mode(s) enumerated from the connected display's EDID/DRM mode list");
    for (size_t i = 0; i < allModes.size() && i < 10; ++i)
        data.push_back("  " + allModes[i]);
    data.push_back("Pi5 controller capability: 640x480 up to 3840x2160 (4K UHD) at 60 Hz");
    {
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
}

// ---------------------------------------------------------------------------
// 5. testVariousRefreshRates
// ---------------------------------------------------------------------------
TestOutput DisplayInterfaces::testVariousRefreshRates()
{
    std::vector<std::string> data;

    data.push_back("Refresh rate support check (Pi5 HDMI 2.0: 23.98-120 Hz, 4K up to 60 Hz)");

    if (!dispDrmAvailable()) {
        data.push_back("[SKIP] DRM sysfs not available — cannot read EDID to measure refresh rates");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    // Acceptance criteria (measurable): the connected sink's EDID detailed
    // timing descriptors must parse into at least one valid vertical refresh
    // rate (pixel clock / (h_total * v_total)). No sink -> SKIPPED.
    static const char* const ports[] = {"HDMI-A-1", "HDMI-A-2"};
    std::string connectedBase;
    for (const char* hdmi : ports) {
        std::string base = dispHdmiConnectorBase(hdmi);
        if (!base.empty() && dispReadFile(base + "/status") == "connected") {
            connectedBase = base;
            break;
        }
    }

    if (connectedBase.empty()) {
        data.push_back("[SKIP] No connected HDMI sink — refresh rates cannot be measured without a display");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    std::vector<double> rates;
    int parsed = dispEdidRefreshRates(connectedBase + "/edid", rates);
    if (parsed <= 0) {
        data.push_back("[FAIL] EDID present but no detailed timing descriptor parsed into a valid vertical refresh rate");
        TestOutput _out;
        _out.status = hal::Status::FAIL;
        _out.severity = hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[PASS] EDID detailed timings yield " + std::to_string(parsed) + " valid vertical refresh rate(s)");
    for (size_t i = 0; i < rates.size() && i < 10; ++i)
        data.push_back("  " + std::to_string(rates[i]) + " Hz");
    data.push_back("Supported controller range: 23.98/24/25/29.97/30/50/59.94/60 Hz (4K), up to 120 Hz at 1080p");
    {
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
}

// ---------------------------------------------------------------------------
// 6. testHdrSupport
// ---------------------------------------------------------------------------
TestOutput DisplayInterfaces::testHdrSupport()
{
    std::vector<std::string> data;

    data.push_back("HDR support check (Pi5 HDMI 2.0 — HDR10 static metadata via InfoFrame)");

    if (!dispDrmAvailable()) {
        data.push_back("[SKIP] DRM sysfs not available — HDR capability cannot be observed");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    // Acceptance criteria (measurable): the KMS driver must expose the HDR
    // output metadata property on an HDMI connector for the Pi5 HDMI path to
    // be HDR-capable. Connector present but property absent = FAIL.
    static const char* const ports[] = {"HDMI-A-1", "HDMI-A-2"};
    bool anyConnector = false, hdrCapable = false, connected = false;
    for (const char* hdmi : ports) {
        std::string base = dispHdmiConnectorBase(hdmi);
        if (base.empty())
            continue;
        anyConnector = true;
        if (dispReadFile(base + "/status") == "connected")
            connected = true;
        if (access((base + "/hdr_output_metadata").c_str(), F_OK) == 0) {
            hdrCapable = true;
            data.push_back(std::string(hdmi) + ": hdr_output_metadata property exposed");
        } else {
            data.push_back(std::string(hdmi) + ": no hdr_output_metadata property (no HDR support exposed by driver)");
        }
    }

    if (!anyConnector) {
        data.push_back("[SKIP] No HDMI connector nodes found in DRM — HDR capability cannot be checked");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    std::string hdrEnabled = dispRunCmd("cat /sys/class/drm/card*-HDMI*/hdr_output_metadata 2>/dev/null");
    if (!hdrEnabled.empty())
        data.push_back("HDR output metadata: " + hdrEnabled.substr(0, 200));
    if (!connected)
        data.push_back("[INFO] No sink currently connected — capability check only");

    if (hdrCapable) {
        data.push_back("[PASS] Driver exposes HDR output metadata interface (HDR10 static metadata path available)");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[FAIL] HDMI connector(s) present but the KMS driver does not expose HDR output metadata — no HDR support on this DUT");
    TestOutput _out;
    _out.status = hal::Status::FAIL;
    _out.severity = hal::Severity::WARNING;
    StampDiagnostics(_out, data);
    return _out;
}

// ---------------------------------------------------------------------------
// 7. testDisplayHotPlugDetection
// ---------------------------------------------------------------------------
TestOutput DisplayInterfaces::testDisplayHotPlugDetection()
{
    std::vector<std::string> data;

    data.push_back("Display hot-plug detection (HPD) check — Pi5 micro-HDMI (RP1 HPD interrupt)");

    if (!dispDrmAvailable()) {
        data.push_back("[SKIP] DRM sysfs not available — HPD status cannot be observed");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    // Acceptance criteria (measurable): HPD works when every present HDMI
    // connector reports a live, HPD-derived status (connected/disconnected)
    // through the DRM connector property. No connector = SKIP; no valid
    // status at all = FAIL.
    static const char* const ports[] = {"HDMI-A-1", "HDMI-A-2"};
    int connectors = 0, withStatus = 0;
    for (const char* hdmi : ports) {
        std::string base = dispHdmiConnectorBase(hdmi);
        if (base.empty()) {
            data.push_back(std::string(hdmi) + ": connector node not found");
            continue;
        }
        ++connectors;
        std::string status = dispReadFile(base + "/status");
        if (status == "connected" || status == "disconnected" || status == "unknown") {
            ++withStatus;
            data.push_back(std::string(hdmi) + ": HPD-derived status '" + status + "'");
        } else {
            data.push_back(std::string(hdmi) + ": status unreadable/invalid (" + (status.empty() ? std::string("empty") : status) + ")");
        }
    }

    if (connectors == 0) {
        data.push_back("[SKIP] No HDMI connector nodes found in DRM — HPD path cannot be checked");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    if (withStatus == 0) {
        data.push_back("[FAIL] No HDMI connector reports a valid HPD status (connected/disconnected)");
        TestOutput _out;
        _out.status = hal::Status::FAIL;
        _out.severity = hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }

    // Kernel HPD event evidence when the ring buffer is readable.
    std::string hpdDmesg = dispRunCmd("dmesg 2>/dev/null | grep -i 'hdmi\\|hotplug\\|hpd' | tail -5");
    if (!hpdDmesg.empty())
        data.push_back("dmesg HPD events: " + hpdDmesg.substr(0, 300));

    data.push_back("[PASS] " + std::to_string(withStatus) + " of " + std::to_string(connectors) +
                   " HDMI connector(s) expose live HPD status via DRM");
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
std::vector<TestResult> DisplayInterfaces::runAllTests()
{
    std::vector<TestResult> results;

    // Run testDisplayPortOutput
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testDisplayPortOutput();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testDisplayPortOutput";
            _tr.test_id = "testDisplayPortOutput";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testHdmiOutput
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testHdmiOutput();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testHdmiOutput";
            _tr.test_id = "testHdmiOutput";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testMultiDisplaySupport
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testMultiDisplaySupport();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testMultiDisplaySupport";
            _tr.test_id = "testMultiDisplaySupport";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testVariousResolutions
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testVariousResolutions();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testVariousResolutions";
            _tr.test_id = "testVariousResolutions";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testVariousRefreshRates
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testVariousRefreshRates();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testVariousRefreshRates";
            _tr.test_id = "testVariousRefreshRates";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testHdrSupport
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testHdrSupport();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testHdrSupport";
            _tr.test_id = "testHdrSupport";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testDisplayHotPlugDetection
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testDisplayHotPlugDetection();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testDisplayHotPlugDetection";
            _tr.test_id = "testDisplayHotPlugDetection";
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
std::vector<std::string> DisplayInterfaces::getAvailableTests()
{
    return {
        "testDisplayPortOutput",   "testHdmiOutput", "testMultiDisplaySupport",     "testVariousResolutions",
        "testVariousRefreshRates", "testHdrSupport", "testDisplayHotPlugDetection",
    };
}

/**
 * @brief Run a single test by name
 * @param testName Name of the test to run
 * @return Test result with timing
 */
TestResult DisplayInterfaces::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput result;
    result.status = hal::Status::ERROR;
    result.severity = hal::Severity::ERROR;
    result.error_message = "test not dispatched";

    if (testName == "testDisplayPortOutput") {
        result = testDisplayPortOutput();
    } else if (testName == "testHdmiOutput") {
        result = testHdmiOutput();
    } else if (testName == "testMultiDisplaySupport") {
        result = testMultiDisplaySupport();
    } else if (testName == "testVariousResolutions") {
        result = testVariousResolutions();
    } else if (testName == "testVariousRefreshRates") {
        result = testVariousRefreshRates();
    } else if (testName == "testHdrSupport") {
        result = testHdrSupport();
    } else if (testName == "testDisplayHotPlugDetection") {
        result = testDisplayHotPlugDetection();
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
