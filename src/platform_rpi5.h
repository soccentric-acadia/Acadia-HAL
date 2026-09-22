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
 * Compute Module 5 (CM5) is the same BCM2712 SoC in a 55×40 mm SoM with a
 * different peripheral budget (see the CM5 constants below):
 *   RAM    : 2/4/8/16 GB LPDDR4X-4267 with ECC
 *   Storage: on-board eMMC (0 GB Lite / 16/32/64 GB, up to 400 MB/s)
 *   PCIe   : Gen 2 x1 (5 Gbps)
 *   USB    : 2× USB 3.0 + 1× USB 2.0
 *   Ethernet: 1 GbE with IEEE 1588 PTP (BCM54210PE PHY)
 *   RTC    : on-board (VBAT battery-backed)
 *   Fan    : Fan_PWM + Fan_Tacho
 *   Wireless: optional CYW43455 802.11ac + BT 5.0 (SKU-dependent)
 *
 * The CM5 has no on-board HDMI/USB/GPIO-header connectors — those live on the
 * carrier (CM5IO) board, so CM5 constants below are used for the peripherals
 * the module itself carries, plus the device-tree model string for detection.
 *
 * @author sandesh ghimire
 */

#pragma once

#include <cstdlib>
#include <fstream>
#include <string>

// ---------------------------------------------------------------------------
// CPU — Quad-core Cortex-A76
// ---------------------------------------------------------------------------
inline constexpr int RPI5_CPU_CORES = 4;
inline constexpr double RPI5_CPU_MAX_FREQ_MHZ = 2400.0; ///< 2.4 GHz stock, up to 3.0 GHz OC
inline constexpr double RPI5_CPU_MIN_FREQ_MHZ = 1500.0; ///< energy-save floor
inline constexpr double RPI5_CPU_TEMP_MAX_C = 85.0;     ///< throttle onset

// ---------------------------------------------------------------------------
// GPU — VideoCore VII
// ---------------------------------------------------------------------------
inline constexpr int RPI5_GPU_SHADER_CORES = 12;       ///< 12 shader quad cores
inline constexpr double RPI5_GPU_MAX_FREQ_MHZ = 800.0; ///< stock max
inline constexpr double RPI5_GPU_MIN_FREQ_MHZ = 300.0;
inline constexpr double RPI5_GPU_TEMP_MAX_C = 85.0;
// Sysfs/devfs paths for VideoCore VII
inline constexpr const char* RPI5_GPU_DEVFREQ_PATH = "/sys/class/devfreq/fde00000.gpu";
inline constexpr const char* RPI5_GPU_DEVICE_PATH = "/sys/devices/platform/fde00000.gpu";
inline constexpr const char* RPI5_V3D_DEV_PATH = "/dev/dri/card0";
inline constexpr const char* RPI5_V3D_RENDER_PATH = "/dev/dri/renderD128";

// ---------------------------------------------------------------------------
// Memory — LPDDR4X
// ---------------------------------------------------------------------------
inline constexpr long long RPI5_MEM_8GB_BYTES = 8LL * 1024 * 1024 * 1024;
inline constexpr long long RPI5_MEM_4GB_BYTES = 4LL * 1024 * 1024 * 1024;
inline constexpr double RPI5_MEM_SPEED_MHZ = 4267.0; ///< LPDDR4X-4267
inline constexpr double RPI5_MEM_BW_GBS = 17.0;      ///< ~17 GB/s theoretical
inline constexpr int RPI5_MEM_BUS_BITS = 64;

// ---------------------------------------------------------------------------
// PCIe — Gen 3 x1 via RP1
// ---------------------------------------------------------------------------
inline constexpr int RPI5_PCIE_GEN = 3;
inline constexpr int RPI5_PCIE_LANES = 1;
inline constexpr double RPI5_PCIE_BW_GBS = 1.0; ///< ~1 GB/s (Gen3 x1, 8b10b)
// RP1 PCIe root complex sysfs
inline constexpr const char* RPI5_PCIE_RC_PATH = "/sys/bus/pci/devices/0000:00:00.0";

// ---------------------------------------------------------------------------
// USB — via RP1 xHCI controller
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_XHCI_DRIVER_PATH = "/sys/bus/platform/drivers/xhci-hcd";
// Two USB 3.0 (5 Gbps) + two USB 2.0 ports
inline constexpr int RPI5_USB3_PORT_COUNT = 2;
inline constexpr int RPI5_USB2_PORT_COUNT = 2;

// ---------------------------------------------------------------------------
// Ethernet — BCM2712 built-in GbE
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_ETH_IFACE = "eth0";
inline constexpr int RPI5_ETH_SPEED_MBPS = 1000;

// ---------------------------------------------------------------------------
// Camera — MIPI CSI-2 via RP1 Unicam (up to 2 cameras simultaneously)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_CSI_DEVICE0 = "/dev/video0";
inline constexpr const char* RPI5_CSI_DEVICE1 = "/dev/video1";
inline constexpr const char* RPI5_MEDIA_CTL0 = "/dev/media0";
// RP1 Unicam platform device
inline constexpr const char* RPI5_UNICAM_PATH = "/sys/bus/platform/devices/1f00110000.csi";

// ---------------------------------------------------------------------------
// Display — HDMI + DSI via RP1
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_DRM_CARD0 = "/dev/dri/card0";
inline constexpr const char* RPI5_DRM_CARD1 = "/dev/dri/card1";

// ---------------------------------------------------------------------------
// Audio — I2S via RP1
// ---------------------------------------------------------------------------
// RP1 I2S / PCM sysfs device
inline constexpr const char* RPI5_I2S_DEVICE = "/sys/bus/platform/devices/1f000a8000.i2s";
// Fallback: ALSA card path
inline constexpr const char* RPI5_ALSA_CARDS = "/proc/asound/cards";

// ---------------------------------------------------------------------------
// Debug / Serial — PL011 UART (AMA0 is the primary debug UART on Pi5)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_DEBUG_UART = "/dev/ttyAMA0";
// Secondary UART (mini-UART is /dev/ttyS0 but PL011 is preferred)
inline constexpr const char* RPI5_UART1 = "/dev/ttyAMA10";

// ---------------------------------------------------------------------------
// Thermal — BCM2712 thermal zones
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_THERMAL_CPU_ZONE = "/sys/class/thermal/thermal_zone0";
inline constexpr const char* RPI5_THERMAL_SB_ZONE = "/sys/class/thermal/thermal_zone1";
inline constexpr double RPI5_THERMAL_THROTTLE_C = 80.0;
inline constexpr double RPI5_THERMAL_SHUTDOWN_C = 85.0;

// ---------------------------------------------------------------------------
// GPIO — RP1 GPIO controller (40-pin header, BCM numbering)
// ---------------------------------------------------------------------------
// RP1 exposes GPIO via /dev/gpiochip4 (chip 0 for header pins on Pi5)
inline constexpr const char* RPI5_GPIOCHIP_PATH = "/dev/gpiochip4";
inline constexpr int RPI5_GPIO_PIN_COUNT = 28; ///< user-accessible header pins

// ---------------------------------------------------------------------------
// Storage paths
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_MMCBLK_PATH = "/dev/mmcblk0"; ///< SD card
inline constexpr const char* RPI5_NVME_PATH = "/dev/nvme0n1";   ///< NVMe (optional)

// ---------------------------------------------------------------------------
// Power — PMIC / power supply sysfs (Pi 5 uses MXL7704 PMIC)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_POWER_SUPPLY_PATH = "/sys/class/power_supply";

// -------------------------------------------------------------------------
// I2C — RP1 I2C controllers (/dev/i2c-1 = I2C1 on header pins 2/5, 3/5)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_I2C_DEV1 = "/dev/i2c-1";
inline constexpr const char* RPI5_I2C_DEV0 = "/dev/i2c-0";
inline constexpr const char* RPI5_I2C_SYSFS = "/sys/bus/i2c/devices";
inline constexpr const char* RPI5_I2C_ADAPTERS = "/sys/class/i2c-adapter";

// ---------------------------------------------------------------------------
// SPI — RP1 SPI controllers (/dev/spidev*)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_SPI_DEV0_0 = "/dev/spidev0.0";
inline constexpr const char* RPI5_SPI_DEV0_1 = "/dev/spidev0.1";
inline constexpr const char* RPI5_SPI_DEV1_0 = "/dev/spidev1.0";
inline constexpr const char* RPI5_SPI_SYSFS = "/sys/class/spi_master";

// ---------------------------------------------------------------------------
// RTC — /dev/rtc0 if a HAT RTC is present (no on-board RTC on Pi5)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_RTC_DEV0 = "/dev/rtc0";
inline constexpr const char* RPI5_RTC_CLASS = "/sys/class/rtc";

// ---------------------------------------------------------------------------
// CAN — no native CAN on Pi5; typically an SPI/MCP2515 HAT exposes can0
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_CAN_IFACE = "can0";
inline constexpr const char* RPI5_CAN_SYSFS = "/sys/class/net";

// ---------------------------------------------------------------------------
// RS-485 — dedicated half-duplex port; must NOT be the debug console
// ---------------------------------------------------------------------------
// HAL-501: /dev/ttyAMA0 is the debug console UART. RS-485 owns a different
// port (default: the secondary UART). The active port is overridable at
// runtime via the ACADIA_RS485_UART environment variable so integrations can
// point it at whatever UART the RS-485 transceiver is wired to.
inline constexpr const char* RPI5_RS485_UART = "/dev/ttyAMA10";
inline const char* acadia_rs485_uart()
{
    const char* env = getenv("ACADIA_RS485_UART");
    return (env && *env) ? env : RPI5_RS485_UART;
}

// ---------------------------------------------------------------------------
// Analog input — IIO ADC devices (e.g. MCP3008 on SPI, or RP1 ADC)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_IIO_SYSFS = "/sys/bus/iio/devices";

// ---------------------------------------------------------------------------
// PWM output — Linux PWM sysfs (/sys/class/pwm)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_PWM_SYSFS = "/sys/class/pwm";
inline constexpr const char* RPI5_PWMCHIP0 = "/sys/class/pwm/pwmchip0";

// ---------------------------------------------------------------------------
// Bluetooth — Pi5 built-in BCM43455 radio (or USB dongle)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_BT_HCI0 = "/dev/hci0";
inline constexpr const char* RPI5_BT_SYSFS = "/sys/class/bluetooth";

// ---------------------------------------------------------------------------
// MIPI DSI — RP1 DSI controller (secondary display)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_DSI_PATH = "/sys/bus/platform/devices/1f00130000.dsi";
inline constexpr const char* RPI5_DSI_DRM = "/sys/class/drm";

// ---------------------------------------------------------------------------
// SD card / eMMC — generic MMC block devices
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_SD_BLOCK_PATH = "/sys/class/mmc_host";
inline constexpr const char* RPI5_EMMC_BLOCK_PATH = "/sys/class/mmc_host";

// ---------------------------------------------------------------------------
// Battery — power supply sysfs (MXL7704 PMIC / HAT battery)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_BATTERY_SUPPLY = "/sys/class/power_supply";

// ---------------------------------------------------------------------------
// Deep-learning / AI accelerator — none on base Pi5
// (External options: Hailo-8 PCIe, Coral USB, etc.)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_HAILO8_PCIE_VENDOR = "1e60"; ///< Hailo PCIe vendor ID
inline constexpr const char* RPI5_CORAL_USB_VENDOR = "18d1";   ///< Google/Goral USB VID

// ---------------------------------------------------------------------------
// Wi-Fi — built-in BCM2805 (802.11ac dual-band) on Pi5 / CM5 (RPI_WIFI)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_WLAN_IFACE = "wlan0";
inline constexpr const char* RPI5_NET_SYSFS = "/sys/class/net";
inline constexpr const char* RPI5_WIRELESS_PROC = "/proc/net/wireless";

// ---------------------------------------------------------------------------
// HDMI — RP1 HDMI ports (Pi5 has 2× HDMI; CM5 up to 2 via carrier)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_HDMI_DRM0 = "/sys/class/drm/card0-HDMI-A-1";
inline constexpr const char* RPI5_HDMI_DRM1 = "/sys/class/drm/card1-HDMI-A-1";

// ---------------------------------------------------------------------------
// Fan — PWM fan on the Pi5 5-pin fan connector (via cooling_device / pwm)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_FAN_THERMAL_CD0 = "/sys/class/thermal/cooling_device0";
inline constexpr const char* RPI5_FAN_THERMAL_CD1 = "/sys/class/thermal/cooling_device1";
inline constexpr const char* RPI5_FAN_PWM = "/sys/class/pwm";

// ---------------------------------------------------------------------------
// Status LEDs — /sys/class/leds (Pi5: LED0=ACT, LED1=PWR)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_LEDS_SYSFS = "/sys/class/leds";

// ---------------------------------------------------------------------------
// HAT / board EEPROM — HAT ID EEPROM in device-tree (vendor/product/uuid)
// ---------------------------------------------------------------------------
inline constexpr const char* RPI5_HAT_DT = "/proc/device-tree/hat";
inline constexpr const char* RPI5_BOARD_EEPROM_PATH = "/sys/bus/i2c/devices";

// ---------------------------------------------------------------------------
// Platform identification
// ---------------------------------------------------------------------------
inline constexpr const char* PLATFORM_NAME = "Raspberry Pi 5";
inline constexpr const char* PLATFORM_SOC = "BCM2712";
inline constexpr const char* PLATFORM_IOBRIDGE = "RP1";
// The Pi 5 device-tree model string
inline constexpr const char* RPI5_DT_MODEL_PATH = "/proc/device-tree/model";

// ---------------------------------------------------------------------------
// Compute Module 5 (CM5) — same BCM2712/RP1 silicon in an SoM form factor
// ---------------------------------------------------------------------------
// CM5 shares the RP1 south-bridge and device-tree paths with the Pi 5; the
// differences that matter to the HAL are storage (eMMC vs microSD), PCIe Gen,
// USB port count, an on-board RTC, and optional wireless depending on SKU.
inline constexpr const char* CM5_DT_MODEL_PATH = "/proc/device-tree/model";
// DT model strings the firmware writes for CM5 / CM5Lite / CM5IO boards
inline constexpr const char* CM5_DT_MODEL_STD = "Raspberry Pi Compute Module 5";
inline constexpr const char* CM5_DT_MODEL_LITE = "Raspberry Pi Compute Module 5 Lite";
inline constexpr const char* CM5IO_DT_MODEL = "Raspberry Pi Compute Module 5 IO Board";
// CM5 RAM options (LPDDR4X-4267, ECC): 2/4/8/16 GB
inline constexpr long long CM5_MEM_16GB_BYTES = 16LL * 1024 * 1024 * 1024;
inline constexpr long long CM5_MEM_8GB_BYTES = 8LL * 1024 * 1024 * 1024;
inline constexpr long long CM5_MEM_4GB_BYTES = 4LL * 1024 * 1024 * 1024;
inline constexpr long long CM5_MEM_2GB_BYTES = 2LL * 1024 * 1024 * 1024;
// eMMC capacity options (CM5 only; 0 == CM5Lite, boots external SD/eMMC)
inline constexpr long long CM5_EMMC_64GB_BYTES = 64LL * 1024 * 1024 * 1024;
inline constexpr long long CM5_EMMC_32GB_BYTES = 32LL * 1024 * 1024 * 1024;
inline constexpr long long CM5_EMMC_16GB_BYTES = 16LL * 1024 * 1024 * 1024;
inline constexpr long long CM5_EMMC_0GB_BYTES = 0LL;
// CM5 eMMC runs up to 400 MB/s (4× CM4)
inline constexpr double CM5_EMMC_BW_MBPS = 400.0;

// CM5 controller / peripheral deltas vs the Pi 5
inline constexpr int CM5_PCIE_GEN = 2; ///< Gen 2 x1 (5 Gbps); Gen3 unsupported
inline constexpr int CM5_USB3_PORT_COUNT = 2;
inline constexpr int CM5_USB2_PORT_COUNT = 1;
inline constexpr int CM5_GPIO_PIN_COUNT = 28;

// CM5 on-board battery-backed RTC (VBAT). No RTC device on the module is
// addressable directly — a carrier RTC (or the kernel RTC driver) exposes it.
inline constexpr const char* CM5_RTC_CLASS = "/sys/class/rtc";

// CM5 fan controller — Fan_PWM (GPIO) + Fan_Tacho (RP1 GPIO29)
inline constexpr const char* CM5_FAN_PWM = "/sys/class/pwm";

// CM5 wireless is optional (SKU-dependent) — CYW43455 on wlan0 when fitted
inline constexpr const char* CM5_WLAN_IFACE = "wlan0";

// Board-family detection helper
enum class PlatformFamily
{
    RPI5,
    CM5,
    CM5_LITE,
    UNKNOWN
};

// Task 64: runtime board detection. Reads the device-tree model string to
// decide which capability table applies, instead of mixing Pi5/CM5/carrier
// assumptions. Returns RPI5 for a Pi 5, CM5/CM5_LITE for Compute Module 5,
// and UNKNOWN when the model is not recognized (callers should not assume
// hardware is present).
inline PlatformFamily DetectPlatformFamily()
{
    std::ifstream f(RPI5_DT_MODEL_PATH);
    if (!f.is_open()) return PlatformFamily::UNKNOWN;
    std::string model;
    std::getline(f, model);
    if (model.find("Raspberry Pi 5") != std::string::npos) return PlatformFamily::RPI5;
    if (model.find("Compute Module 5 Lite") != std::string::npos) return PlatformFamily::CM5_LITE;
    if (model.find("Compute Module 5") != std::string::npos) return PlatformFamily::CM5;
    return PlatformFamily::UNKNOWN;
}

// Task 64: capability table driven by runtime detection. Declares, per family,
// whether on-board RTC, CAN, wireless, and PCIe are present — so module
// behavior no longer assumes a fixed board.
struct PlatformCapabilities {
    bool rtc{false};
    bool can{false};        // native CAN (Pi5/CM5 have none; SPI MCP2515 HAT only)
    bool wireless{false};
    bool pcie{false};
    int  gpio_pin_count{0};
};

inline PlatformCapabilities DetectCapabilities(PlatformFamily family)
{
    PlatformCapabilities caps;
    switch (family) {
        case PlatformFamily::RPI5:
            caps.rtc = false;                 // no on-board RTC on Pi5 (HAT only)
            caps.can = false;                 // MCP2515 HAT only
            caps.wireless = true;             // built-in CYW43439
            caps.pcie = true;                 // Gen 3 x1 via RP1
            caps.gpio_pin_count = 28;
            break;
        case PlatformFamily::CM5:
        case PlatformFamily::CM5_LITE:
            caps.rtc = true;                  // on-board VBAT RTC
            caps.can = false;
            caps.wireless = false;            // SKU-dependent, on carrier
            caps.pcie = true;                 // Gen 2 x1
            caps.gpio_pin_count = 28;
            break;
        default:
            caps.gpio_pin_count = 0;
            break;
    }
    return caps;
}
