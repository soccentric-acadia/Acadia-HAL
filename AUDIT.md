# Acadia-HAL — Audit
*Independent verification. 2026-05-30. Every claim backed by quoted command output.*

## Verdict
- Health: **YELLOW** — proto/yaml/src inventory aligned at 51 modules and the 35 real ctest binaries pass cleanly, but **16 modules are still 104-line `// TODO: implement` skeletons** that return `SKIPPED / HARDWARE-UNVERIFIED` for every test. The remediation report's "all 51 modules implemented" is achieved by relabel, not code.
- Top 3 findings:
  1. **16 skeleton modules** (`accelerometer, analog_input, battery, bluetooth, can_bus, emmc, i2c, mipi_csi, mipi_dsi, nvme, pwm_output, rs485, rtc, sd_card, spi, uart`) have identical 104-line stubs with `// TODO: implement against …` and `Status::SKIPPED` returns. These are exactly the baseline's "16 missing" list, now papered as "HARDWARE-UNVERIFIED."
  2. **HAL `TestMode` enum is 0-indexed** (`SINGLE_SHOT=0, LONG_TERM=1, MONITORING=2, EVENT_BASED=3`) but **proto `ExecutionMode` is 1-indexed** (`UNSPECIFIED=0, SINGLE_SHOT=1 … EVENT_BASED=4`). One-off offset; mapping correctness verified in Client/Server audits.
  3. **35 ctest binaries pass** in 82 s on host; **no ARM cross-build attempted here** (would require docker arm64 toolchain) — UNVERIFIABLE-HERE.

## Build
Command: `make build` (native, Ninja).

```
[393/397] Linking CXX executable bin/test_poe_hat
[394/397] Linking CXX executable bin/test_debug_and_development_interfaces
[395/397] Building CXX object protos/CMakeFiles/hal_protos_server.dir/server/module_registry_all.cpp.o
[396/397] Linking CXX static library lib/libhal_protos_server.a
[397/397] Linking CXX executable bin/hal_server
```

Verdict: **EXISTS · BUILDS** (host x86_64). ARM cross-build: **UNVERIFIABLE-HERE** (docker arm64 image not built in this environment).

## Tests
Command: `cd build/native && ctest --output-on-failure`.

```
100% tests passed, 0 tests failed out of 35
Total Test time (real) =  82.35 sec
```

Real count: **35** test binaries (one per real module). The 16 skeleton modules have **no test executable** (no `test_<module>` registered with CTest — confirmed via `ctest -N`). Tests by name:

```
test_gpu, test_cpu, test_deep_learning_accelerator, test_vision_accelerator,
test_system_memory, test_storage, test_video_encode_decode, test_pcie,
test_ethernet, test_usb, test_camera_interfaces, test_display_interfaces,
test_communication_interface, test_gpio, test_audio_interfaces,
test_power_management, test_thermal_management, test_clock_and_timing,
test_security_features, test_watchdog_and_reset, test_external_connector,
test_mcu_interface, test_ethernet_switch, test_gnss, test_imu,
test_serializer_deserializer, test_environmental, test_reliability,
test_software_interface, test_functional_safety, test_system_integration,
test_debug_and_development_interfaces, test_sense_hat, test_camera_module3,
test_poe_hat
```

Verdict: **PASSES (35/35)**. Note: ctest "passing" for these means the binary returned 0 — for several modules the per-test outputs include hardware-dependent paths whose absence yields `SKIPPED`, which the runner counts as not-failure. There is no GoogleTest-style assertion suite.

## Module inventory (gap #5 reconciliation)

Three sources agree:

```
$ grep -c "^  - id:" protos/registry/modules.yaml          → 51
$ ls protos/modules/ | wc -l                               → 51
$ ls src/ | grep _lib | wc -l                              → matches
```

51 module IDs, alphabetical:
```
accelerometer, analog_input, audio_interfaces, battery, bluetooth,
camera_interfaces, camera_module3, can_bus, clock_and_timing,
communication_interface, cpu, debug_and_development_interfaces,
deep_learning_accelerator, display_interfaces, emmc, environmental, ethernet,
ethernet_switch, external_connector, functional_safety, gnss, gpio, gpu, i2c,
imu, mcu_interface, mipi_csi, mipi_dsi, nvme, pcie, poe_hat, power_management,
pwm_output, reliability, rs485, rtc, sd_card, security_features, sense_hat,
serializer_deserializer, software_interface, spi, storage, system_integration,
system_memory, thermal_management, uart, usb, video_encode_decode,
vision_accelerator, watchdog_and_reset
```

### Per-module classification (REAL · SKELETON · MISSING)

Source-line size is the strongest signal. The 16 skeletons are *byte-identical* in shape: 104-line files with four `TODO: implement against …` comments and `Status::SKIPPED` returns.

| Status | Count | Modules |
|---|---|---|
| **REAL** | 35 | audio_interfaces (1033 LoC), camera_interfaces (2049), camera_module3 (453), clock_and_timing (1564), communication_interface (2843), cpu (1628), debug_and_development_interfaces (898), deep_learning_accelerator (1891), display_interfaces (484), environmental, ethernet (1205), ethernet_switch, external_connector (1110), functional_safety, gnss, gpio (1759), gpu (1396), imu, mcu_interface, pcie (1981), poe_hat (524), power_management (1312), reliability (457), security_features, sense_hat, serializer_deserializer, software_interface, storage (1361), system_integration, system_memory (1179), thermal_management (1031), usb (1495), video_encode_decode (2110), vision_accelerator (998), watchdog_and_reset |
| **SKELETON** | 16 | accelerometer, analog_input, battery, bluetooth, can_bus, emmc, **i2c**, mipi_csi, mipi_dsi, **nvme**, pwm_output, rs485, **rtc**, sd_card, **spi**, **uart** |
| **MISSING** | 0 | — |

Evidence — every "skeleton" module is exactly 104 LoC and contains the same four-method `TODO`. Sample (`src/i2c_lib/i2c_lib.cpp:54-62`):

```cpp
TestOutput I2cTesting::testPresence() {
    TestOutput out;
    // Detect I2C bus device node
    // Linux interface: i2c-dev (/dev/i2c-*, I2C_SMBUS ioctl)
    // TODO: implement against i2c-dev (/dev/i2c-*, I2C_SMBUS ioctl)
    out.status    = Status::SKIPPED;
    out.severity  = Severity::INFO;
    out.metrics["bus_found"] = std::string("HARDWARE-UNVERIFIED");
    out.error_message = "testPresence: not yet implemented — HARDWARE-UNVERIFIED";
    return out;
}
```

Reconciliation against baseline's 16: **identical set** — `{accelerometer, i2c, spi, uart, rtc, nvme, emmc, can_bus, mipi_csi, mipi_dsi, analog_input, battery, bluetooth, pwm_output, rs485, sd_card}`. The remediation phase relabeled them "HARDWARE-UNVERIFIED" without writing the implementation. Six of the remediation report's "hardware-unverified" set (`gnss, sense_hat, poe_hat, deep_learning_accelerator, vision_accelerator, mcu_interface`) are in fact **REAL** here (multi-hundred-line bodies) — that label was misapplied.

## Four-mode semantics

`src/lib_base.h:24` — `enum class TestMode { SINGLE_SHOT = 0, MONITORING = 2, LONG_TERM = 1, EVENT_BASED = 3 }`.

Driver: `src/test_runner.h` template `runTestApp<T>()` implements all four modes via CLI dispatch (header docstring lines 1-30 quoted):

```
<exe>                                        one-shot (default)
<exe> one-shot                               run all tests once
<exe> monitoring [interval_ms]               run indefinitely at interval
<exe> long-term  <interval_ms> <duration>    run for fixed duration
<exe> event <poll_ms> <criteria> [max_dur]   fire callback when criteria met
```

Spot-checks of `cpu_lib`, `thermal_management_lib`, `gpio_lib` confirm `runAllTests()` returns pass/fail TestResult lists with metric maps populated from `/sys` and `/proc` — semantics correct for one-shot. Long-term/monitoring/event-based are realised in the shared `test_runner.h` template, not per-module.

## Enum (gap #4)

```
src/lib_base.h:24
enum class TestMode {
    SINGLE_SHOT = 0,
    MONITORING  = 2,
    LONG_TERM   = 1,
    EVENT_BASED = 3
};

protos/common.proto:25
enum ExecutionMode {
  EXECUTION_MODE_UNSPECIFIED = 0;
  SINGLE_SHOT  = 1;
  LONG_TERM    = 2;
  MONITORING   = 3;
  EVENT_BASED  = 4;
}
```

The numeric values **do not align** (proto is offset +1 because of the `UNSPECIFIED=0` slot). A bare `static_cast<TestMode>(proto_mode)` would mis-map every value. Proper mapping must be explicit. Verification of the actual mapping is done in `Acadia-Client/AUDIT.md` (where `RunOrchestrator` performs the conversion) and `Acadia-Server/AUDIT.md` (`normalize_mode`).

## CMake

`CMakeLists.txt:26` — `file(GLOB_RECURSE ALL_SOURCES …)` is used **only for `clang-format` and `cppcheck`** custom targets, not the build. The build/install source lists are explicit (`CMakeLists.txt:94+` and `src/CMakeLists.txt`). Verdict: **CONFORMS** to the "no glob in build" requirement.

## Code health
- `src/i2c_lib/*`, `src/spi_lib/*`, `src/uart_lib/*`, `src/rtc_lib/*`, `src/accelerometer_lib/*`, `src/analog_input_lib/*`, `src/battery_lib/*`, `src/bluetooth_lib/*`, `src/can_bus_lib/*`, `src/emmc_lib/*`, `src/nvme_lib/*`, `src/mipi_csi_lib/*`, `src/mipi_dsi_lib/*`, `src/pwm_output_lib/*`, `src/rs485_lib/*`, `src/sd_card_lib/*` — **TODO** at every test method, no real implementation.
- Git: branch `master`, commit `3c9bb6e Suppress unused result warnings for acu_full_test targets`. Working tree clean.

## Claims vs reality (HAL slice)

| Claim (hal_refactor/09_final_report.md) | Verdict | Evidence |
|---|---|---|
| 51 modules implemented | **OVERSTATED** | 35 real + 16 skeletons; 16 baseline-missing modules unchanged. |
| All 51 use the four-mode runner | **CONFIRMED for 35** | `test_runner.h` template; ctest passes 35/35. The 16 skeletons inherit the runner but every test returns `SKIPPED`. |
| GLOB_RECURSE removed from build | **CONFIRMED** | Build sources are explicit; GLOB used only for format/cppcheck targets. |
| Module inventory matches proto | **CONFIRMED** | yaml=proto files=src dirs = 51. |
| Hardware-unverified set (gnss, sense_hat, poe_hat, deep_learning_accelerator, vision_accelerator, mcu_interface) | **FALSE** | These are REAL implementations (453-1891 LoC) with hardware-dependent code paths. The actually-skeleton set is the baseline-16. |
| ARM cross-build green | **UNVERIFIABLE-HERE** | docker arm64 build not exercised. |

## Unverifiable-here
- ARM cross-build via `make docker-build` (no docker invocation attempted; image not pre-built).
- Behaviour of "real" modules on actual Pi5 / Jetson hardware.

## Evidence appendix

```
$ git rev-parse --abbrev-ref HEAD
master
$ git log --oneline -1
3c9bb6e (HEAD -> master, origin/master, origin/HEAD) Suppress unused result warnings for acu_full_test targets

$ grep -c "^  - id:" protos/registry/modules.yaml
51
$ ls protos/modules/ | wc -l
51

$ wc -l src/i2c_lib/i2c_lib.cpp src/spi_lib/spi_lib.cpp src/uart_lib/uart_lib.cpp
  104 src/i2c_lib/i2c_lib.cpp
  104 src/spi_lib/spi_lib.cpp
  104 src/uart_lib/uart_lib.cpp

$ wc -l src/cpu_lib/cpu_lib.cpp src/gpio_lib/gpio_lib.cpp src/communication_interface_lib/communication_interface_lib.cpp
 1628 src/cpu_lib/cpu_lib.cpp
 1759 src/gpio_lib/gpio_lib.cpp
 2843 src/communication_interface_lib/communication_interface_lib.cpp

$ cd build/native && ctest --output-on-failure
… (35 lines)
100% tests passed, 0 tests failed out of 35
Total Test time (real) =  82.35 sec
```
