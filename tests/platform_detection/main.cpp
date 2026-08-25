// test_platform_detection — task 64: runtime board detection + capability
// tables drive module behavior; no mixed Pi5/CM5/carrier assumptions.
#include "platform_rpi5.h"
#include <cstdio>

int g_failures = 0;
#define EXPECT(cond, msg) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s\n", msg); ++g_failures; } } while (0)

int main() {
    // Capability tables must be distinct per family.
    auto rpi5 = DetectCapabilities(PlatformFamily::RPI5);
    auto cm5  = DetectCapabilities(PlatformFamily::CM5);
    EXPECT(rpi5.rtc == false, "RPI5 has no on-board RTC (HAT only)");
    EXPECT(cm5.rtc == true, "CM5 has on-board VBAT RTC");
    EXPECT(rpi5.wireless == true, "RPI5 has built-in wireless");
    EXPECT(cm5.wireless == false, "CM5 wireless is SKU/carrier dependent");
    EXPECT(rpi5.can == false, "neither board has native CAN (MCP2515 HAT)");
    EXPECT(rpi5.gpio_pin_count == 28, "RPI5 GPIO count");
    EXPECT(cm5.gpio_pin_count == 28, "CM5 GPIO count");
    // UNKNOWN must not claim any hardware is present.
    auto unknown = DetectCapabilities(PlatformFamily::UNKNOWN);
    EXPECT(unknown.gpio_pin_count == 0, "UNKNOWN board claims no GPIO");
    EXPECT(unknown.rtc == false && unknown.wireless == false,
           "UNKNOWN board claims no RTC/wireless");

    if (g_failures != 0) { std::fprintf(stderr, "\n%d failure(s)\n", g_failures); return 1; }
    std::fprintf(stderr, "\nall platform detection checks passed\n");
    return 0;
}
