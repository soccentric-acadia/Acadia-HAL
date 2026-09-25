/**
 * @file platform_rpi5.h
 * @brief Raspberry Pi 5 (BCM2712 / RP1) platform hardware constants
 *
 * All hardware-specific values for the Raspberry Pi 5 are centralised here.
 * Include this header in any library that was previously hard-coded against
 * NVIDIA Jetson Orin (Tegra234) hardware.
 *
 * Raspberry Pi 5 key specs:
 *   SoC    : Broadcom BCM2712 (quad-core Cortex-A76 @ 2.4 GHz)
 *   I/O    : RP1 south-bridge chip (USB 3, PCIe, MIPI CSI/DSI, I2S, GPIO …)
 *   RAM    : 4 GB or 8 GB LPDDR4X-4267 (64-bit interface)
 *   GPU    : VideoCore VII (no CUDA, no NVENC/NVDEC)
 *   PCIe   : Gen 3 x1 (M.2 M-key slot) via RP1
 *   Storage: microSD + optional NVMe via M.2
 *   USB    : 2× USB 3.0 + 2× USB 2.0 via RP1 xHCI
 *   Ethernet: Gigabit Ethernet (BCM2712 built-in MAC)
 *
 * @author sandesh ghimire
 */

#pragma once

// ---------------------------------------------------------------------------
// CPU — Quad-core Cortex-A76
// ---------------------------------------------------------------------------
inline constexpr int    RPI5_CPU_CORES          = 4;
inline constexpr double RPI5_CPU_MAX_FREQ_MHZ   = 2400.0;  ///< 2.4 GHz stock, up to 3.0 GHz OC
inline constexpr double RPI5_CPU_MIN_FREQ_MHZ   = 1500.0;  ///< energy-save floor
inline constexpr double RPI5_CPU_TEMP_MAX_C     = 85.0;    ///< throttle onset

// ---------------------------------------------------------------------------
// GPU — VideoCore VII
// ---------------------------------------------------------------------------
inline constexpr int    RPI5_GPU_SHADER_CORES   = 12;      ///< 12 shader quad cores
inline constexpr double RPI5_GPU_MAX_FREQ_MHZ   = 800.0;   ///< stock max
inline constexpr double RPI5_GPU_MIN_FREQ_MHZ   = 300.0;
inline constexpr double RPI5_GPU_TEMP_MAX_C     = 85.0;
// Sysfs/devfs paths for VideoCore VII
inline constexpr const char* RPI5_GPU_DEVFREQ_PATH =
    "/sys/class/devfreq/fde00000.gpu";
inline constexpr const char* RPI5_GPU_DEVICE_PATH  =
    "/sys/devices/platform/fde00000.gpu";
inline constexpr const char* RPI5_V3D_DEV_PATH     = "/dev/dri/card0";
inline constexpr const char* RPI5_V3D_RENDER_PATH  = "/dev/dri/renderD128";

// ---------------------------------------------------------------------------
// Memory — LPDDR4X
// ---------------------------------------------------------------------------
inline constexpr long long RPI5_MEM_8GB_BYTES   = 8LL  * 1024 * 1024 * 1024;
inline constexpr long long RPI5_MEM_4GB_BYTES   = 4LL  * 1024 * 1024 * 1024;
inline constexpr double    RPI5_MEM_SPEED_MHZ   = 4267.0;  ///< LPDDR4X-4267
inline constexpr double    RPI5_MEM_BW_GBS      = 17.0;    ///< ~17 GB/s theoretical
inline constexpr int       RPI5_MEM_BUS_BITS    = 64;

// ---------------------------------------------------------------------------
// PCIe — Gen 3 x1 via RP1
// ---------------------------------------------------------------------------
inline constexpr int    RPI5_PCIE_GEN           = 3;
inline constexpr int    RPI5_PCIE_LANES         = 1;
inline constexpr double RPI5_PCIE_BW_GBS        = 1.0;     ///< ~1 GB/s (Gen3 x1, 8b10b)
// RP1 PCIe root complex sysfs
inline constexpr const char* RPI5_PCIE_RC_PATH  = "/sys/bus/pci/devices/0000:00:00.0";

// ---------------------------------------------------------------------------
// USB — via RP1 xHCI controller
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_XHCI_DRIVER_PATH =
    "/sys/bus/platform/drivers/xhci-hcd";
// Two USB 3.0 (5 Gbps) + two USB 2.0 ports
inline constexpr int RPI5_USB3_PORT_COUNT       = 2;
inline constexpr int RPI5_USB2_PORT_COUNT       = 2;

// ---------------------------------------------------------------------------
// Ethernet — BCM2712 built-in GbE
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_ETH_IFACE     = "eth0";
inline constexpr int         RPI5_ETH_SPEED_MBPS = 1000;

// ---------------------------------------------------------------------------
// Camera — MIPI CSI-2 via RP1 Unicam (up to 2 cameras simultaneously)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_CSI_DEVICE0   = "/dev/video0";
inline constexpr const char* RPI5_CSI_DEVICE1   = "/dev/video1";
inline constexpr const char* RPI5_MEDIA_CTL0    = "/dev/media0";
// RP1 Unicam platform device
inline constexpr const char* RPI5_UNICAM_PATH   =
    "/sys/bus/platform/devices/1f00110000.csi";

// ---------------------------------------------------------------------------
// Display — HDMI + DSI via RP1
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_DRM_CARD0     = "/dev/dri/card0";
inline constexpr const char* RPI5_DRM_CARD1     = "/dev/dri/card1";

// ---------------------------------------------------------------------------
// Audio — I2S via RP1
// ---------------------------------------------------------------------------
// RP1 I2S / PCM sysfs device
inline constexpr const char* RPI5_I2S_DEVICE    =
    "/sys/bus/platform/devices/1f000a8000.i2s";
// Fallback: ALSA card path
inline constexpr const char* RPI5_ALSA_CARDS    = "/proc/asound/cards";

// ---------------------------------------------------------------------------
// Debug / Serial — PL011 UART (AMA0 is the primary debug UART on Pi5)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_DEBUG_UART    = "/dev/ttyAMA0";
// integration-ivv.md VB-7 / HW-004: this UART is the Pico W real-time
// co-processor link, NOT a general-purpose "second serial port" — it has
// already been wrongly claimed as the default RS-485/UART test device once
// (a regression on the first HW-004 fix), which would drive the co-processor
// link instead. uart_lib/rs485_lib must NOT default to this path; do not
// rename this back to something generic ("RPI5_UART1") that invites reuse.
inline constexpr const char* RPI5_PICO_LINK_UART = "/dev/ttyAMA10";

// ---------------------------------------------------------------------------
// Thermal — BCM2712 thermal zones
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_THERMAL_CPU_ZONE   = "/sys/class/thermal/thermal_zone0";
inline constexpr const char* RPI5_THERMAL_SB_ZONE    = "/sys/class/thermal/thermal_zone1";
inline constexpr double      RPI5_THERMAL_THROTTLE_C = 80.0;
inline constexpr double      RPI5_THERMAL_SHUTDOWN_C = 85.0;

// ---------------------------------------------------------------------------
// GPIO — RP1 GPIO controller (40-pin header, BCM numbering)
// ---------------------------------------------------------------------------
// RP1 exposes GPIO via /dev/gpiochip4 (chip 0 for header pins on Pi5)
inline constexpr const char* RPI5_GPIOCHIP_PATH = "/dev/gpiochip4";
inline constexpr int         RPI5_GPIO_PIN_COUNT = 28;  ///< user-accessible header pins

// ---------------------------------------------------------------------------
// Storage paths
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_MMCBLK_PATH   = "/dev/mmcblk0";   ///< SD card
inline constexpr const char* RPI5_NVME_PATH     = "/dev/nvme0n1";   ///< NVMe (optional)

// ---------------------------------------------------------------------------
// Power — PMIC / power supply sysfs (Pi 5 uses MXL7704 PMIC)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_POWER_SUPPLY_PATH = "/sys/class/power_supply";

// ---------------------------------------------------------------------------
// Deep-learning / AI accelerator — none on base Pi5
// (External options: Hailo-8 PCIe, Coral USB, etc.)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_HAILO8_PCIE_VENDOR = "1e60"; ///< Hailo PCIe vendor ID
inline constexpr const char* RPI5_CORAL_USB_VENDOR   = "18d1"; ///< Google/Coral USB VID

// ---------------------------------------------------------------------------
// Platform identification
// ---------------------------------------------------------------------------
inline constexpr const char* PLATFORM_NAME      = "Raspberry Pi 5";
inline constexpr const char* PLATFORM_SOC       = "BCM2712";
inline constexpr const char* PLATFORM_IOBRIDGE  = "RP1";
// The Pi 5 device-tree model string
inline constexpr const char* RPI5_DT_MODEL_PATH = "/proc/device-tree/model";
