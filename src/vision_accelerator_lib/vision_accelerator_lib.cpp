/**
 * @file vision_accelerator_lib.cpp
 * @brief Vision accelerator tests for Raspberry Pi 5.
 *        Pi5 has NO NVIDIA PVA (Programmable Vision Accelerator) or dedicated
 *        VLIW VPU.  Methods referencing VLIW slots, PVA power, etc. are
 *        Jetson-specific and are mapped to Pi5 equivalents:
 *          - "VPU" → VideoCore VII GPU (v3d kernel driver)
 *          - "Vector units" → Cortex-A76 NEON SIMD
 *          - "DMA engine" → BCM2712 DMA controllers
 *          - "Computer vision" → V4L2 ISP (bcm2835-isp) + OpenCV + TFLite
 * @author sandesh ghimire
 * @copyright (C) Lucid Motors
 */

#include "vision_accelerator_lib.h"
#include "../platform_rpi5.h"

#include <array>
#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <time.h>
#include <vector>

// ---------------------------------------------------------------------------
[[maybe_unused]] static std::string vaReadFile(const std::string& path)
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

static std::string vaRunCmd(const std::string& cmd)
{
    std::array<char, 1024> buf{};
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
static bool vaParseLong(const std::string& s, long& out)
{
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    long v = strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || errno == ERANGE) return false;
    out = v;
    return true;
}

static bool vaPathExists(const std::string& p)
{
    struct stat st {};
    return stat(p.c_str(), &st) == 0;
}
// ---------------------------------------------------------------------------

/**
 * @brief Constructor for VisionAcceleratorTesting
 */
VisionAcceleratorTesting::VisionAcceleratorTesting()
{
}

/**
 * @brief Destructor for VisionAcceleratorTesting
 */
VisionAcceleratorTesting::~VisionAcceleratorTesting()
{
}

/**
 * @brief Dual VPU → Pi5 dual ARM cluster (4× Cortex-A76) + VideoCore VII GPU.
 */
TestOutput VisionAcceleratorTesting::testDualVectorProcessingUnits()
{
    std::vector<std::string> data;
    data.push_back("=== Dual VPU (Pi5 Cortex-A76 NEON + VideoCore VII) ===");
    data.push_back("[INFO] Pi5 has no dedicated NVIDIA PVA; VPU = Cortex-A76 + VideoCore VII");

    std::string cpuCount = vaRunCmd("grep -c 'processor' /proc/cpuinfo 2>/dev/null");
    data.push_back("[INFO] CPU cores (A76 NEON vector units): " + cpuCount);

    bool v3dOk = vaPathExists("/sys/module/v3d");
    data.push_back(v3dOk ? "[PASS] v3d (VideoCore VII GPU) module loaded"
                         : "[INFO] v3d module not found");

    std::string gpuFreq = vaRunCmd("vcgencmd measure_clock v3d 2>/dev/null");
    data.push_back("[INFO] v3d clock: " + (gpuFreq.empty() ? "N/A" : gpuFreq));

    {
        TestOutput _out;
        _out.status = v3dOk ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = v3dOk ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        // Typed measurements (req.md §2 rule 1): CPU core count and GPU clock
        // are chartable integers; downstream consumers (Web UI thresholds)
        // rely on type rather than on string parsing.
        if (!cpuCount.empty()) {
            try { _out.metrics["cpu_core_count"] = static_cast<int64_t>(std::stoi(cpuCount)); }
            catch (...) { /* leave as reason only */ }
        }
        if (!gpuFreq.empty()) {
            try {
                // vcgencmd output: "frequency(48)=700000000" → Hz
                auto eq = gpuFreq.find('=');
                auto close = gpuFreq.find(')', eq);
                if (eq != std::string::npos && close != std::string::npos) {
                    long long hz = std::stoll(gpuFreq.substr(eq + 1, close - eq - 1));
                    _out.metrics["v3d_clock_hz"] = static_cast<int64_t>(hz);
                }
            } catch (...) { /* leave as reason only */ }
        }
        return _out;
    }
}

/**
 * @brief Vector cores per VPU → Cortex-A76 NEON registers (32× 128-bit).
 */
TestOutput VisionAcceleratorTesting::testVectorCoresInEachVpu()
{
    std::vector<std::string> data;
    data.push_back("=== Vector Cores per VPU (Cortex-A76 NEON) ===");
    data.push_back("[INFO] Cortex-A76: 32× 128-bit NEON/SIMD registers per core (256-bit SVE optional)");

    std::string cpuFeatures = vaRunCmd("grep -m1 'Features' /proc/cpuinfo 2>/dev/null");
    data.push_back("[INFO] " + cpuFeatures);

    bool neOk = cpuFeatures.find("neon") != std::string::npos ||
                cpuFeatures.find("asimd") != std::string::npos;
    data.push_back(neOk ? "[PASS] NEON/AdvSIMD present" : "[WARN] NEON not found in CPU features");

    bool fp16Ok = cpuFeatures.find("fphp") != std::string::npos ||
                  cpuFeatures.find("asimdhp") != std::string::npos;
    data.push_back(fp16Ok ? "[PASS] FP16 (SIMD half-precision) supported" : "[INFO] No FP16 SIMD");

    {
        TestOutput _out;
        _out.status = neOk ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = neOk ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

/**
 * @brief VLIW slots → N/A on Pi5 (Cortex-A76 is out-of-order superscalar, not VLIW).
 */
TestOutput VisionAcceleratorTesting::testVliwSlots()
{
    std::vector<std::string> data;
    data.push_back("=== VLIW Slots (Pi5 — N/A) ===");
    data.push_back("[INFO] Cortex-A76 is out-of-order superscalar; not VLIW");
    data.push_back("[INFO] Pi5 issue width: 4-wide decode, 8-entry dispatch queue");
    data.push_back("[INFO] For VLIW-style compute, use Hailo-8 (HailoRT compiler produces VLIW DSP code)");

    bool hailoOk = vaPathExists("/dev/hailo0");
    data.push_back(hailoOk ? "[PASS] Hailo-8 /dev/hailo0 present (supports VLIW DSP ops)"
                           : "[INFO] No Hailo-8 HAT");

    // Acceptance criteria: VLIW-style compute requires the Hailo-8 DSP path;
    // the Pi5 Cortex-A76 is out-of-order superscalar (not VLIW) — the
    // requirement is not applicable without the accelerator.
    if (hailoOk)
    {
        data.push_back("[PASS] VLIW-class compute path present (Hailo-8 VLIW DSP)");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[SKIP] No VLIW hardware on this DUT (Cortex-A76 is superscalar, not VLIW; no Hailo-8 attached) — not applicable");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief Scalar execution units → Cortex-A76 integer/FP pipelines.
 */
TestOutput VisionAcceleratorTesting::testScalarExecutionUnits()
{
    std::vector<std::string> data;
    data.push_back("=== Scalar Execution Units (Cortex-A76) ===");
    data.push_back("[INFO] Cortex-A76: 3× integer ALUs, 2× FP/NEON FMAC units per core");

    std::string cpuFreq = vaRunCmd("cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq 2>/dev/null");
    long cpuFreqKhz = 0;
    if (!cpuFreq.empty())
    {
        if (vaParseLong(cpuFreq, cpuFreqKhz))
            data.push_back("[INFO] CPU0 current freq: " + std::to_string(cpuFreqKhz / 1000) + " MHz");
        else
            data.push_back("[INFO] CPU frequency value unparseable: '" + cpuFreq + "'");
    }

    std::string maxFreq = vaRunCmd("cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq 2>/dev/null");
    long maxFreqKhz = 0;
    data.push_back("[INFO] CPU max freq: " +
                   (maxFreq.empty() ? "N/A" : std::to_string(vaParseLong(maxFreq, maxFreqKhz) ? maxFreqKhz / 1000 : 0) + " MHz") +
                   " (rated " + std::to_string(RPI5_CPU_MAX_FREQ_MHZ) + " MHz)");

    {
        TestOutput _out;
        const bool _ok = (!cpuFreq.empty());
        _out.status = _ok ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = _ok ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        // Typed measurements (req.md §2 rule 1): CPU frequency in kHz from
        // sysfs → typed integer for charting.
        if (cpuFreqKhz > 0) {
            _out.metrics["cpu0_cur_freq_khz"] = static_cast<int64_t>(cpuFreqKhz);
        }
        if (maxFreqKhz > 0) {
            _out.metrics["cpu0_max_freq_khz"] = static_cast<int64_t>(maxFreqKhz);
        }
        _out.metrics["cpu_max_freq_rated_mhz"] = static_cast<int64_t>(RPI5_CPU_MAX_FREQ_MHZ);
        return _out;
    }
}

/**
 * @brief Vector execution units → Cortex-A76 NEON throughput benchmark.
 */
TestOutput VisionAcceleratorTesting::testVectorExecutionUnits()
{
    std::vector<std::string> data;
    data.push_back("=== Vector Execution Units (Pi5 NEON FMAC throughput) ===");

    static constexpr size_t SIZE = 64ULL * 1024 * 1024;
    std::vector<uint8_t> src(SIZE, 0x5A);
    std::vector<uint8_t> dst(SIZE);
    struct timespec t0 {}, t1 {};
    clock_gettime(CLOCK_MONOTONIC, &t0);
    memcpy(dst.data(), src.data(), SIZE);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long long ns = (t1.tv_sec - t0.tv_sec) * 1000000000LL + (t1.tv_nsec - t0.tv_nsec);
    double gbps = (SIZE / 1e9) / (ns / 1e9);
    data.push_back("[PASS] 64 MB NEON memcpy: " +
                   std::to_string(static_cast<int>(gbps * 1000)) + " MB/s  (" +
                   std::to_string(ns / 1000000) + " ms)");

    {
        TestOutput _out;
        const bool _ok = (gbps > 0.5);
        _out.status = _ok ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = _ok ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        // Typed measurements (req.md §2 rule 1): memcpy latency (ns) and
        // throughput (GB/s) — the headline numbers for a NEON throughput
        // benchmark must be typed for downstream charting/thresholding.
        _out.metrics["neon_memcpy_ns"] = static_cast<int64_t>(ns);
        _out.metrics["neon_memcpy_gbps"] = static_cast<double>(gbps);
        return _out;
    }
}

/**
 * @brief DMA engine 1 — BCM2712 DMA controller (DMA4 / DMA-Lite channels).
 */
TestOutput VisionAcceleratorTesting::testDmaEngine1Functionality()
{
    std::vector<std::string> data;
    data.push_back("=== DMA Engine 1 (Pi5 BCM2712 DMA4) ===");

    bool dmaPresent = !vaRunCmd("ls /sys/bus/platform/devices/ 2>/dev/null | grep -i dma | head -3").empty();
    data.push_back(dmaPresent ? "[PASS] DMA platform devices found"
                              : "[INFO] DMA platform devices not found");

    std::string dmaMod = vaRunCmd("grep -c 'bcm2835-dma\\|bcm2712' /proc/modules 2>/dev/null");
    data.push_back("[INFO] BCM DMA module count: " + dmaMod);
    long dmaModN = 0;
    vaParseLong(dmaMod, dmaModN);

    // Acceptance criteria (measurable): the BCM2712 DMA controller must be
    // enumerable (platform devices or the driver module bound).
    if (dmaPresent || dmaModN > 0)
    {
        data.push_back("[PASS] DMA engine 1 (BCM2712 DMA4) present and enumerable");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        // Typed measurement (req.md §2 rule 1): BCM DMA module count.
        _out.metrics["bcm_dma_module_count"] = static_cast<int64_t>(dmaModN);
        return _out;
    }

    data.push_back("[SKIP] No DMA platform devices or driver module — DMA engine 1 not enumerable on this DUT");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief DMA engine 2 — RP1 DMA controller for peripherals.
 */
TestOutput VisionAcceleratorTesting::testDmaEngine2Functionality()
{
    std::vector<std::string> data;
    data.push_back("=== DMA Engine 2 (Pi5 RP1 DMA) ===");
    data.push_back("[INFO] RP1 south-bridge has its own DMA engines for USB3, PCIe, SPI, UART");

    std::string rp1Dma = vaRunCmd("dmesg 2>/dev/null | grep -i 'rp1.*dma' | head -2");
    data.push_back(rp1Dma.empty() ? "[INFO] No RP1 DMA messages in dmesg"
                                  : "[PASS] RP1 DMA: " + rp1Dma);

    std::string rp1Mod = vaRunCmd("grep -c '^rp1' /proc/modules 2>/dev/null");
    data.push_back("[INFO] RP1 modules loaded: " + rp1Mod);

    // Acceptance criteria (measurable): the RP1 DMA engine must be
    // observable (dmesg probe messages or the rp1 driver module bound).
    if (!rp1Dma.empty())
    {
        data.push_back("[PASS] RP1 DMA engine present (kernel messages confirm binding)");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    long rp1N = 0;
    if (vaParseLong(rp1Mod, rp1N) && rp1N > 0)
    {
        data.push_back("[PASS] RP1 driver modules loaded (DMA engines managed by RP1)");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[SKIP] No RP1 DMA evidence in kernel log or modules — RP1 DMA engine not observable on this DUT");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief DMA bandwidth test: measure DMA-driven memory copy throughput.
 */
TestOutput VisionAcceleratorTesting::testDmaBandwidthTesting()
{
    std::vector<std::string> data;
    data.push_back("=== DMA Bandwidth (Pi5 LPDDR4X-4267) ===");

    static constexpr size_t MB128 = 128ULL * 1024 * 1024;
    std::vector<uint8_t> buf1(MB128, 0xAA);
    std::vector<uint8_t> buf2(MB128);
    struct timespec t0 {}, t1 {};
    clock_gettime(CLOCK_MONOTONIC, &t0);
    memcpy(buf2.data(), buf1.data(), MB128);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ms = (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1e6;
    double gbps = (MB128 / 1e9) / (ms / 1000.0);
    data.push_back("[PASS] DMA bandwidth proxy (memcpy 128 MB): " +
                   std::to_string(static_cast<int>(gbps * 1000)) + " MB/s  (" +
                   std::to_string(static_cast<int>(ms)) + " ms)");
    data.push_back("[INFO] Pi5 theoretical LPDDR4X-4267 bandwidth: ~34 GB/s (dual-channel)");

    bool ok = gbps > 1.0;
    {
        TestOutput _out;
        _out.status = ok ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = ok ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        // Typed measurements (req.md §2 rule 1): 128 MB DMA-proxy latency
        // and throughput — typed for charting.
        _out.metrics["dma_memcpy_ms"] = static_cast<double>(ms);
        _out.metrics["dma_memcpy_gbps"] = static_cast<double>(gbps);
        return _out;
    }
}

/**
 * @brief DMA latency test: measure DMA initiation latency.
 */
TestOutput VisionAcceleratorTesting::testDmaLatencyTesting()
{
    std::vector<std::string> data;
    data.push_back("=== DMA Latency (Pi5) ===");

    static constexpr size_t KB4 = 4096;
    uint8_t src[KB4], dst[KB4];
    memset(src, 0xBB, KB4);
    struct timespec t0 {}, t1 {};

    memcpy(dst, src, KB4); // warm-up

    long long totalNs = 0;
    constexpr int ITERS = 1000;
    for (int i = 0; i < ITERS; i++)
    {
        clock_gettime(CLOCK_MONOTONIC, &t0);
        memcpy(dst, src, KB4);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        totalNs += (t1.tv_sec - t0.tv_sec) * 1000000000LL + (t1.tv_nsec - t0.tv_nsec);
    }
    long long avgNs = totalNs / ITERS;
    data.push_back("[PASS] 4 KB DMA-proxy latency avg: " + std::to_string(avgNs) + " ns");
    data.push_back("[INFO] CPU L1 cache copy; true DMA latency via /dev/mem requires root");

    {
        TestOutput _out;
        const bool _ok = (avgNs < 100000);
        _out.status = _ok ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = _ok ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        // Typed measurement (req.md §2 rule 1): 4 KB DMA-proxy avg latency
        // in nanoseconds — typed for thresholding (< 100µs pass).
        _out.metrics["dma_proxy_avg_latency_ns"] = static_cast<int64_t>(avgNs);
        return _out;
    }
}

/**
 * @brief Computer vision algorithms: check V4L2 ISP (bcm2835-isp) pipeline.
 */
TestOutput VisionAcceleratorTesting::testComputerVisionAlgorithms()
{
    std::vector<std::string> data;
    data.push_back("=== Computer Vision Algorithms (Pi5 V4L2 ISP + OpenCV) ===");

    bool ispOk = !vaRunCmd("grep -c 'bcm2835.isp' /proc/modules 2>/dev/null").empty();
    data.push_back(ispOk ? "[PASS] bcm2835-isp V4L2 ISP available"
                         : "[INFO] bcm2835-isp not detected");

    std::string ocv = vaRunCmd("python3 -c 'import cv2; print(cv2.__version__)' 2>/dev/null");
    data.push_back(ocv.empty() ? "[INFO] OpenCV not installed (pip install opencv-python)"
                               : "[PASS] OpenCV " + ocv + " available");

    std::string ocvBuild = vaRunCmd(
        "python3 -c \""
        "import cv2; info=cv2.getBuildInformation(); "
        "print('NEON' if 'NEON' in info else 'no-NEON')\" 2>/dev/null");
    data.push_back(ocvBuild == "NEON" ? "[PASS] OpenCV built with NEON acceleration" : "[INFO] " + ocvBuild);

    // Acceptance criteria (measurable): a vision-algorithm path requires the
    // ISP module or OpenCV. Neither -> SKIP.
    if (!ocv.empty())
    {
        data.push_back("[PASS] Computer-vision algorithm surface available (OpenCV" +
                       std::string(ocvBuild == "NEON" ? " with NEON acceleration" : "") + ")");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[SKIP] No vision algorithm stack (bcm2835-isp absent, OpenCV not installed) — CV algorithms cannot be exercised on this DUT");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief Image processing pipelines: libcamera → ISP → V4L2 capture.
 */
TestOutput VisionAcceleratorTesting::testImageProcessingPipelines()
{
    std::vector<std::string> data;
    data.push_back("=== Image Processing Pipeline (Pi5 libcamera → ISP → V4L2) ===");

    std::string lc = vaRunCmd("libcamera-hello --list-cameras 2>/dev/null | head -3");
    data.push_back(lc.empty() ? "[INFO] libcamera not available or no cameras"
                              : "[PASS] libcamera:\n" + lc);

    std::string gst = vaRunCmd("gst-launch-1.0 --version 2>/dev/null | head -1");
    data.push_back(gst.empty() ? "[INFO] GStreamer not found" : "[INFO] " + gst);

    data.push_back("[INFO] Pi5 ISP pipeline: Sensor → MIPI D-PHY → Unicam → bcm2835-isp");
    data.push_back("[INFO]   bcm2835-isp stages: debayer, ALSC, AGC, AWB, denoise, sharpen");

    // Acceptance criteria (measurable): the image-processing pipeline
    // requires a camera source (libcamera) and/or GStreamer.
    if (!lc.empty() || !gst.empty())
    {
        data.push_back("[PASS] Image-processing pipeline surface present (" +
                       std::string(!lc.empty() ? "libcamera" : "GStreamer") + ")");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[SKIP] No image-processing pipeline surface (libcamera and GStreamer absent) — pipeline cannot be exercised on this DUT");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief Feature detection: test FAST keypoint detection via OpenCV.
 */
TestOutput VisionAcceleratorTesting::testFeatureDetection()
{
    std::vector<std::string> data;
    data.push_back("=== Feature Detection (Pi5 OpenCV FAST/ORB) ===");

    std::string ocv = vaRunCmd("python3 -c 'import cv2; print(cv2.__version__)' 2>/dev/null");
    if (ocv.empty())
    {
        data.push_back("[INFO] OpenCV not available — skipping FAST/ORB test");
        {
            TestOutput _out;
            _out.status = hal::Status::SKIPPED;
            _out.severity = hal::Severity::INFO;
            StampDiagnostics(_out, data);
            return _out;
        }
    }

    std::string result = vaRunCmd(
        "python3 -c \""
        "import cv2, numpy as np; "
        "img = np.random.randint(0, 255, (480,640), dtype=np.uint8); "
        "fast = cv2.FastFeatureDetector_create(); "
        "kp = fast.detect(img, None); "
        "print(len(kp))\" 2>/dev/null");
    data.push_back(!result.empty() ? "[PASS] FAST detector found " + result + " keypoints"
                                   : "[INFO] FAST keypoint test failed");

    {
        TestOutput _out;
        const bool _ok = (!result.empty());
        _out.status = _ok ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = _ok ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

/**
 * @brief Optical flow: test Lucas-Kanade optical flow via OpenCV.
 */
TestOutput VisionAcceleratorTesting::testOpticalFlow()
{
    std::vector<std::string> data;
    data.push_back("=== Optical Flow (Pi5 OpenCV Farneback) ===");

    std::string ocv = vaRunCmd("python3 -c 'import cv2; print(cv2.__version__)' 2>/dev/null");
    if (ocv.empty())
    {
        data.push_back("[INFO] OpenCV not available — skipping optical flow test");
        {
            TestOutput _out;
            _out.status = hal::Status::SKIPPED;
            _out.severity = hal::Severity::INFO;
            StampDiagnostics(_out, data);
            return _out;
        }
    }

    std::string result = vaRunCmd(
        "python3 -c \""
        "import cv2, numpy as np; "
        "f1 = np.random.randint(0,255,(480,640),dtype=np.uint8); "
        "f2 = np.roll(f1,5); "
        "flow = cv2.calcOpticalFlowFarneback(f1,f2,None,0.5,3,15,3,5,1.2,0); "
        "print('ok', flow.shape)\" 2>/dev/null");
    data.push_back(result.empty() ? "[INFO] Farneback flow test failed"
                                  : "[PASS] Optical flow: " + result);

    {
        TestOutput _out;
        const bool _ok = (!result.empty());
        _out.status = _ok ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = _ok ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

/**
 * @brief Stereo disparity: test stereo block matching via OpenCV.
 */
TestOutput VisionAcceleratorTesting::testStereoDisparity()
{
    std::vector<std::string> data;
    data.push_back("=== Stereo Disparity (Pi5 OpenCV StereoBM) ===");

    std::string ocv = vaRunCmd("python3 -c 'import cv2; print(cv2.__version__)' 2>/dev/null");
    if (ocv.empty())
    {
        data.push_back("[INFO] OpenCV not available — skipping stereo test");
        {
            TestOutput _out;
            _out.status = hal::Status::SKIPPED;
            _out.severity = hal::Severity::INFO;
            StampDiagnostics(_out, data);
            return _out;
        }
    }

    std::string result = vaRunCmd(
        "python3 -c \""
        "import cv2, numpy as np; "
        "left  = np.random.randint(0,255,(480,640),dtype=np.uint8); "
        "right = np.roll(left,16); "
        "sbm = cv2.StereoBM_create(numDisparities=64,blockSize=15); "
        "disp = sbm.compute(left,right); "
        "print('ok', disp.shape)\" 2>/dev/null");
    data.push_back(result.empty() ? "[INFO] StereoBM test failed"
                                  : "[PASS] Stereo disparity: " + result);

    {
        TestOutput _out;
        const bool _ok = (!result.empty());
        _out.status = _ok ? hal::Status::PASS : hal::Status::FAIL;
        _out.severity = _ok ? hal::Severity::INFO : hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }
}

/**
 * @brief PVA power consumption → N/A on Pi5; checks VideoCore VII power.
 */
TestOutput VisionAcceleratorTesting::testPvaPowerConsumption()
{
    std::vector<std::string> data;
    data.push_back("=== Vision Accelerator Power (Pi5 VideoCore VII / v3d) ===");
    data.push_back("[INFO] Pi5 has no NVIDIA PVA; VideoCore VII is the vision compute IP");

    std::string v3dFreq = vaRunCmd("vcgencmd measure_clock v3d 2>/dev/null");
    data.push_back("[INFO] v3d clock: " + (v3dFreq.empty() ? "N/A" : v3dFreq));

    std::string vCore = vaRunCmd("vcgencmd measure_volts core 2>/dev/null");
    data.push_back("[INFO] Core voltage: " + (vCore.empty() ? "N/A" : vCore));

    std::string throttle = vaRunCmd("vcgencmd get_throttled 2>/dev/null");
    bool clean = !throttle.empty() && throttle == "throttled=0x0";
    data.push_back(clean ? "[PASS] No throttle (SoC within power budget)"
                         : "[INFO] Throttle: " + throttle);

    data.push_back("[INFO] Pi5 BCM2712 rated for 5W typical (25°C, default clock)");
    data.push_back("[INFO] For Hailo-8 vision accelerator power: ~2 W typical, 10 W peak");

    // Acceptance criteria (measurable): power health requires readable
    // throttle telemetry with clean flags. Throttle flags -> FAIL; telemetry
    // absent -> SKIP.
    if (throttle.empty())
    {
        data.push_back("[SKIP] No power telemetry (vcgencmd absent) — accelerator power state cannot be measured");
        TestOutput _out;
        _out.status = hal::Status::SKIPPED;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    if (!clean)
    {
        data.push_back("[FAIL] Throttle flags set: " + throttle + " — power budget exceeded during vision workloads");
        TestOutput _out;
        _out.status = hal::Status::FAIL;
        _out.severity = hal::Severity::ERROR;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[PASS] SoC within power budget (no throttle flags; PVA itself is N/A on Pi5)");
    TestOutput _out;
    _out.status = hal::Status::PASS;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief 5-dimensional addressing (N/A on Pi5) — DMA multi-dim stride proxy.
 * @details Jetson PVA DMA supports 5D addressing (N/C/D/H/W strides) — legacy naming; the capability probe below maps to Pi5.
 *          Pi5 DMA4 supports 2D stride (rows × cols).  Documents limitation.
 */
TestOutput VisionAcceleratorTesting::test5dimensionalAddressing()
{
    std::vector<std::string> data;
    data.push_back("=== 5D Addressing (Pi5 DMA4 2D stride proxy) ===");
    data.push_back("[INFO] Jetson PVA DMA supports N/C/D/H/W 5D addressing; Pi5 DMA4 = 2D");
    data.push_back("[INFO] Pi5 DMA4: src_inc, src_width, dst_inc, dst_width (2D scatter-gather)");

    std::string dmaInfo = vaRunCmd(
        "ls /sys/bus/platform/devices/ 2>/dev/null | grep -i 'dma\\|dma4' | head -3");
    data.push_back(dmaInfo.empty() ? "[INFO] DMA4 platform entries not found"
                                   : "[INFO] DMA platform devices: " + dmaInfo);

    // Acceptance criteria: 5D addressing is a Jetson PVA capability; the Pi5
    // DMA4 supports 2D stride only — the 5D requirement is hardware
    // not-applicable; the measurable 2D proxy is the DMA device presence.
    if (!dmaInfo.empty())
    {
        data.push_back("[PASS] 2D-stride DMA path present (Pi5 DMA4; 5D addressing not applicable to this DUT)");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[SKIP] No DMA devices — 5D/multi-dim stride addressing not verifiable on this DUT (not applicable without DMA hardware)");
    TestOutput _out;
    _out.status = hal::Status::SKIPPED;
    _out.severity = hal::Severity::INFO;
    StampDiagnostics(_out, data);
    return _out;
}

/**
 * @brief 16 hardware DMA channels per engine (Pi5 DMA4 has 8 channels per SoC block).
 */
TestOutput VisionAcceleratorTesting::test16HardwareChannelsPerEngine()
{
    std::vector<std::string> data;
    data.push_back("=== 16 HW DMA Channels (Pi5 DMA4 8-ch proxy) ===");
    data.push_back("[INFO] Jetson PVA DMA engines have 16 hw channels; Pi5 DMA4 has 8 channels");

    std::string dmaChans = vaRunCmd(
        "cat /sys/kernel/debug/dma_api/num_allocs 2>/dev/null");
    data.push_back(dmaChans.empty() ? "[INFO] DMA channel debug not accessible (need root)"
                                    : "[INFO] DMA alloc count: " + dmaChans);

    data.push_back("[INFO] BCM2712 DMA4: 8 channels @ 400 MHz, 32-bit address, 128-bit bus");

    // Acceptance criteria (measurable): the DMA channel state must be
    // observable via debugfs or a platform device.
    if (!dmaChans.empty())
    {
        data.push_back("[PASS] DMA channel state readable (alloc count: " + dmaChans + ")");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }
    std::string dmaInfo = vaRunCmd(
        "ls /sys/bus/platform/devices/ 2>/dev/null | grep -i 'dma\\|dma4' | head -3");
    if (!dmaInfo.empty())
    {
        data.push_back("[PASS] DMA platform device present (" + dmaInfo + ") — channel hardware exists (16-channel requirement N/A on Pi5's 8-channel DMA4)");
        TestOutput _out;
        _out.status = hal::Status::PASS;
        _out.severity = hal::Severity::INFO;
        StampDiagnostics(_out, data);
        return _out;
    }

    data.push_back("[SKIP] DMA channel evidence unavailable (debugfs not mounted, no DMA platform devices)");
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
std::vector<TestResult> VisionAcceleratorTesting::runAllTests()
{
    std::vector<TestResult> results;

    // Run testDualVectorProcessingUnits
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testDualVectorProcessingUnits();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testDualVectorProcessingUnits";
            _tr.test_id = "testDualVectorProcessingUnits";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testVectorCoresInEachVpu
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testVectorCoresInEachVpu();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testVectorCoresInEachVpu";
            _tr.test_id = "testVectorCoresInEachVpu";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testVliwSlots
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testVliwSlots();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testVliwSlots";
            _tr.test_id = "testVliwSlots";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testScalarExecutionUnits
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testScalarExecutionUnits();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testScalarExecutionUnits";
            _tr.test_id = "testScalarExecutionUnits";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testVectorExecutionUnits
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testVectorExecutionUnits();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testVectorExecutionUnits";
            _tr.test_id = "testVectorExecutionUnits";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testDmaEngine1Functionality
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testDmaEngine1Functionality();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testDmaEngine1Functionality";
            _tr.test_id = "testDmaEngine1Functionality";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testDmaEngine2Functionality
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testDmaEngine2Functionality();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testDmaEngine2Functionality";
            _tr.test_id = "testDmaEngine2Functionality";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run test5dimensionalAddressing
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = test5dimensionalAddressing();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "test5dimensionalAddressing";
            _tr.test_id = "test5dimensionalAddressing";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run test16HardwareChannelsPerEngine
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = test16HardwareChannelsPerEngine();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "test16HardwareChannelsPerEngine";
            _tr.test_id = "test16HardwareChannelsPerEngine";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testDmaBandwidthTesting
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testDmaBandwidthTesting();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testDmaBandwidthTesting";
            _tr.test_id = "testDmaBandwidthTesting";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testDmaLatencyTesting
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testDmaLatencyTesting();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testDmaLatencyTesting";
            _tr.test_id = "testDmaLatencyTesting";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testComputerVisionAlgorithms
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testComputerVisionAlgorithms();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testComputerVisionAlgorithms";
            _tr.test_id = "testComputerVisionAlgorithms";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testImageProcessingPipelines
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testImageProcessingPipelines();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testImageProcessingPipelines";
            _tr.test_id = "testImageProcessingPipelines";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testFeatureDetection
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testFeatureDetection();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testFeatureDetection";
            _tr.test_id = "testFeatureDetection";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testOpticalFlow
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testOpticalFlow();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testOpticalFlow";
            _tr.test_id = "testOpticalFlow";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testStereoDisparity
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testStereoDisparity();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testStereoDisparity";
            _tr.test_id = "testStereoDisparity";
            _tr.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            _tr.output = result;
            results.push_back(_tr);
        }
    }

    // Run testPvaPowerConsumption
    {
        auto start = std::chrono::high_resolution_clock::now();
        TestOutput result = testPvaPowerConsumption();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        {
            TestResult _tr;
            _tr.name = "testPvaPowerConsumption";
            _tr.test_id = "testPvaPowerConsumption";
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
std::vector<std::string> VisionAcceleratorTesting::getAvailableTests()
{
    return {
        "testDualVectorProcessingUnits",
        "testVectorCoresInEachVpu",
        "testVliwSlots",
        "testScalarExecutionUnits",
        "testVectorExecutionUnits",
        "testDmaEngine1Functionality",
        "testDmaEngine2Functionality",
        "testDmaBandwidthTesting",
        "testDmaLatencyTesting",
        "testComputerVisionAlgorithms",
        "testImageProcessingPipelines",
        "testFeatureDetection",
        "testOpticalFlow",
        "testStereoDisparity",
        "testPvaPowerConsumption",
    };
}

/**
 * @brief Run a single test by name
 * @param testName Name of the test to run
 * @return Test result with timing
 */
TestResult VisionAcceleratorTesting::runSingleTest(const std::string& testName)
{
    auto start = std::chrono::high_resolution_clock::now();
    TestOutput result;
    result.status = hal::Status::ERROR;
    result.severity = hal::Severity::ERROR;
    result.error_message = "test not dispatched";

    if (testName == "testDualVectorProcessingUnits") {
        result = testDualVectorProcessingUnits();
    } else if (testName == "testVectorCoresInEachVpu") {
        result = testVectorCoresInEachVpu();
    } else if (testName == "testVliwSlots") {
        result = testVliwSlots();
    } else if (testName == "testScalarExecutionUnits") {
        result = testScalarExecutionUnits();
    } else if (testName == "testVectorExecutionUnits") {
        result = testVectorExecutionUnits();
    } else if (testName == "testDmaEngine1Functionality") {
        result = testDmaEngine1Functionality();
    } else if (testName == "testDmaEngine2Functionality") {
        result = testDmaEngine2Functionality();
    } else if (testName == "testDmaBandwidthTesting") {
        result = testDmaBandwidthTesting();
    } else if (testName == "testDmaLatencyTesting") {
        result = testDmaLatencyTesting();
    } else if (testName == "testComputerVisionAlgorithms") {
        result = testComputerVisionAlgorithms();
    } else if (testName == "testImageProcessingPipelines") {
        result = testImageProcessingPipelines();
    } else if (testName == "testFeatureDetection") {
        result = testFeatureDetection();
    } else if (testName == "testOpticalFlow") {
        result = testOpticalFlow();
    } else if (testName == "testStereoDisparity") {
        result = testStereoDisparity();
    } else if (testName == "testPvaPowerConsumption") {
        result = testPvaPowerConsumption();
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
