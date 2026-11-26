# Bug Report — Acadia-HAL

Full-file review, one file at a time. Severity: **HIGH** = crash / UB / data race / wrong result; **MED** = misbehavior at an edge case or silently-wrong data; **LOW** = cosmetic / doc / minor hazard.

---

## src/lib_base.h

| # | Sev | Finding |
|---|-----|---------|
| 1 | MED | `StampDiagnostics()` doc says each line is "parsed as a number when possible … consumers read a typed Value", but the implementation always stores `::hal::Value{data[i]}` as a **string** — the number-parsing never happens. Either the promise is unimplemented or the doc is lying. |
| 2 | MED | `StampDiagnostics()` writes `out.metrics["metric_N"]` with a positional index, overwriting any real metric a module already stored under the same key, and clobbering across repeated calls (stale `metric_7` from a previous call is never cleared). |
| 3 | LOW | Global-namespace `using` injections (`using hal::TestResult;` etc.) pollute every TU that includes this header — collision risk with other libraries that define `Status`/`Value`. |
| 4 | LOW | `ModuleRunConfig::run_forever` duplicates the meaning of `duration_ms == 0` (documented as "0 = run until stop()") — two fields expressing one concept invites inconsistent configs. |
| 5 | LOW | Typo in `pause()` doc: "Has no effect on SINGLE_SHOT / SINGLE_SHOT tests" (repeated word). |

## src/lib_base.cpp

| # | Sev | Finding |
|---|-----|---------|
| 1 | **HIGH** | `runSelectedTests()` timeout path: the worker thread is **detached** but captured **`&` by reference** — the local `TestResult result`, the `std::promise done`, and everything in scope can be destroyed while the detached thread is still running `runSingleTest()` and later executes `result = ...` / `done.set_value(true)`. That is a data race / use-after-free (UB). The "abandoned worker" trade-off must copy the result instead (e.g., worker writes to a heap-owned box) or the promise must survive. |
| 2 | **HIGH** | Same call: on timeout the main thread does `result = std::move(err)` and `results.push_back(std::move(result))` while the detached worker may concurrently assign to `result` — two threads writing one object with no synchronization. |
| 3 | MED | `sleepUntil()`/loop sleep accepts a negative or zero `interval_ms`/`poll_ms` (nothing validates the parameters of `runTests`, `runLongTerm`, `runMonitoringTest`, `runEventTest`). A negative interval makes the deadline in the past → **busy loop** with effectively no sleep. |
| 4 | MED | `stop()` join is guarded against the thread stopping itself, but `waitWhilePaused()` can hold `wake_mu_` while the loop is paused; a `stop()` *from another thread* wakes it correctly, yet `monitoring_thread_.detach()` in `stop()` means the detached loop keeps running after `stop()` returns — code that immediately re-`runTests()` can observe two loops (new thread + zombie detached one) racing on the same object. |
| 5 | MED | Destructor waits on `cycles_in_flight_` with a 3-second cap, but a hung test (see bug #1) never decrements it; the destructor then calls `stop()`, joins a thread **still inside `runAllTestsSafe()`**, and the derived object is destroyed underneath the running test → UB. (The `stop()` join has itself no timeout.) |
| 6 | LOW | `runSelectedTests()` catches exceptions from `runSingleTest` but a test that throws between cycles of `runAllTests()` in MONITORING mode only gets the wrapped ERROR result — fine, but the `done.set_value(true)` in the worker runs **after** the catch-blocks write to `result`; if `runSingleTest` throws while writing to `result` mid-way, state is partially updated. Minor. |
| 7 | LOW | `isRunning()` returns `!stopped_`, which is `true` immediately after starting the background thread even if the thread crashed/exited in the first ms; also `true` before `stopped_=true` is written at thread exit is racy (though `std::atomic`). Display-only, low impact. |

## src/platform_rpi5.h

| # | Sev | Finding |
|---|-----|---------|
| 1 | MED | `RPI5_UART1` (Pico W co-processor link) and `RPI5_RS485_UART` both resolve to **the same device `/dev/ttyAMA10`** — two different protocols (Pico W link, RS-485) are hard-wired to one port and will collide if both modules are used. |
| 2 | MED | `acadia_rs485_uart()` calls `getenv()` — returns a pointer to a static buffer that can be invalidated by a concurrent `setenv()`, and is not async-signal-safe; also no path validation of the user-supplied env value. |
| 3 | LOW | Pi 5 debug console is `ttyS0` (PL011); the file uses `/dev/ttyAMA0` and calls it "the primary debug UART". On current Pi5 firmware the console device is usually `ttyS0`, so detection logic keyed on `ttyAMA0` may miss the console. |
| 4 | LOW | `DetectCapabilities()` comment says RPI5 has "built-in CYW43439" but the RPi5 ships the **CYW43455** (even this file's BLUETOOTH section says BCM43455/CYW43455). Inconsistent chip claims. |
| 5 | LOW | `CM5_DT_MODEL_PATH` duplicates `RPI5_DT_MODEL_PATH` (same string) — redundant symbol. |
| 6 | LOW | `DetectPlatformFamily()` reads only the first line of the DT model with `std::getline` and never checks the stream error state; NUL bytes in the DT string are tolerated accidentally. Fragile but works in practice. |

## src/test_runner.h

| # | Sev | Finding |
|---|-----|---------|
| 1 | **HIGH** | `trPi5MonitorItems()` lambdas call `std::stoi`/`std::stoll`/`std::stod` on arbitrary sysfs contents **without try/catch inside the reader**; the base-class catch makes it survivable, but any sysfs returning non-numeric text makes the monitor readout "error" *and* — worse — the exception is thrown from a std::function call in the background loop, so on strict stdlib/error paths this masks real telemetry silently. |
| 2 | MED | `trParseDuration()` silently accepts malformed input via `std::stoll` partial parsing: `"1d2"` parses as `1` (plain-integer fallback), `"5x30s"` as `5x` → 5 ms. No reject on trailing garbage. |
| 3 | MED | `runTestApp()` parses `interval_ms`/`poll_ms` with `std::stoi` but **never rejects 0 or negative** values → combined with `lib_base` bug (#3) this is a busy-loop / infinite fast-spin from the public CLI. |
| 4 | MED | `trEvaluateEventExpression()`: the operator scan order checks single-char `>` / `<` **after** `>=`/`<=`, but a metric name containing `>` or `<` (legal in the identifier check? no — but a RHS like "1e5" is fine; an expr such as `a < b > 5`) parses the **first** `>` encountered even when the intended operator is `<`; ambiguous expressions give wrong evaluation instead of rejection. |
| 5 | MED | `trEvaluateEventExpression()` compares with `value == threshold` using raw float equality — `fail-rate`-style numeric comparisons on `stod` results are unreliable for `==`; should use epsilon. |
| 6 | MED | `trWaitForCompletion()` polls `tester.isRunning()` every 200 ms; when the user hits Ctrl+C the flag is set, the loop exits, and `tester.stop()` is called once — but if a *new* mode was started in the meantime (impossible via CLI, possible via the API), `g_test_stop_flag = false` is reset **globally** even though other threads may still rely on the previous stop request. Design smell. |
| 7 | LOW | `trPrintTable()` counts `ABORTED` under `errored` — aborts are a distinct category; totals under-report. |
| 8 | LOW | `static std::atomic<bool> g_test_stop_flag` in a header — if a TU includes it from >1 translation unit inside the same binary, each copy is independent (documented "static is fine" only because one TU per binary). Fragile contract. |
| 9 | LOW | `trMakeCriteria("fail-rate-above-")` threshold parsing uses `stoi` which accepts `"100abc"` as 100 (partial parse) — invalid suffix silently accepted. |
| 10 | LOW | One-shot mode (`runTestApp`) does **not** `std::signal(SIGINT, ...)` — Ctrl+C kills the test mid-run without any teardown of derived-class resources (contrast with the other modes which install the handler before starting the background loop). |

---
*Review in progress — files continue below.*

## src/accelerometer_lib/accelerometer_lib.h

| # | Sev | Finding |
|---|-----|---------|
| 1 | LOW | `~AccelerometerTesting()` is declared non-virtual; combined with `TestBase` virtual destructor it's fine, but the derived dtor doesn't call `stop()` — a live monitoring thread will join in the base dtor, which blocks client threads up to 3 s while doing a full test cycle. Acceptable but worth knowing. |
| 2 | LOW | Header exposes internal test methods (`testAccelerometerPresent`) as public API — anyone can bypass `runSingleTest` dispatch and thus skip the HAL-014 per-test watchdog. |

## src/accelerometer_lib/accelerometer_lib.cpp

| # | Sev | Finding |
|---|-----|---------|
| 1 | MED | **Missing timestamps on results** (systemic, applies to every module): `runAllTests()`/`runSingleTest()` never set `TestResult::timestamp`. The base only stamps it inside `runSelectedTests()`, so results produced via `runTests(SINGLE_SHOT)` / MONITORING / LONG_TERM all carry `timestamp == epoch(1970)`, contradicting the "HAL-007: stamp timestamp at base level" contract (the stamp lives in the wrong function — `runAllTestsSafe()` should do it). |
| 2 | MED | `testAccelerometerRead()` returns PASS if **any one** of x/y/z raw files is non-empty — a device exposing only `in_accel_x_raw` (or a stale/zero file) passes as a full 3-axis read. Should require all three axes. |
| 3 | MED | `testAccelerometerRead()` treats a file that exists but contains `error`/`failed` (common IIO error string) as a successful read (`!v.empty()` check). IIO sysfs returns the literal string `error` on failure, which is non-empty → false PASS. |
| 4 | LOW | `testAccelerometerScale()` reads first non-empty match across **all** IIO devices without cross-checking the device actually has accel channels — a non-accel device exposing an `in_accel_scale`-like file would pass the test. |
| 5 | LOW | `runSingleTest()` reports unknown names as ERROR with `testId = testName` but `name = testName + " (UNKNOWN)"` — name and id diverge, downstream correlation by name will break. |
## src/analog_input_lib/analog_input_lib.h

| # | Sev | Finding |
|---|-----|---------|
| 1 | LOW | Same pattern: public test methods bypass the HAL-014 watchdog; non-virtual dtor; no `noexcept` or spec on thread-safety. |

## src/analog_input_lib/analog_input_lib.cpp

| # | Sev | Finding |
|---|-----|---------|
| 1 | MED | `testAdcDevicePresent()` treats **any** IIO device as an ADC (an accelerometer, light sensor, etc. in `/sys/bus/iio/devices` satisfies it) → false PASS for "ADC present" with no ADC attached. The comment says the comment scans for ADC specifically but the code only checks the `iio:device` prefix. |
| 2 | MED | `testAdcChannelRead()` passes on the literal IIO error string: if the raw file contains `error` (kernel's IIO failure sentinel) it is non-empty → counted as a successful read, and marked PASS. |
| 3 | MED | The same "file exists ⇒ readable ⇒ PASS" applies here (see accelerometer #2/#3) — this test never distinguishes `0` (valid) from `error` (failure). |
| 4 | MED | `testAdcResolution()` conflates `in_voltage_scale` (volts per LSB) with resolution (bit count) and reports either as the resolution — wrong metric, mislabeled result. |
| 5 | LOW | No `timestamp` stamp (systemic, see lib_base #1 in accelerometer section). |

## src/audio_interfaces/audio_interfaces.h

| # | Sev | Finding |
|---|-----|---------|
| 1 | LOW | No bugs in the header itself; class exposes all test methods publicly (see systemic note). |

## src/audio_interfaces/audio_interfaces.cpp

| # | Sev | Finding |
|---|-----|---------|
| 1 | MED | `testAudioSampleRates()`: the pass/fail outcome is a **tautology**. `requiredRates` is checked only against the hardcoded `orinRates` list (which is static data always containing the required set), never against anything read from the actual hardware. `foundRates` is collected from `hw_params` but **never used** — dead code. The test can only fail if the code is edited to disagree with itself; it tells you nothing about the real device. |
| 2 | MED | `testAudioLatency()`: same tautology — the 10 ms latency check only executes when an audio stream is **already active** (hw_params exists); otherwise prints "no active stream" and still passes. A PASS/FAIL result on "automotive latency ≤ 10 ms" is therefore never actually measured by this test as written. |
| 3 | MED | `testAudioBitDepths()`: checks `fmt.find(bits)` against the *hardcoded* supported-formats list (`orinFormats`) rather than any real device data — always-true; "S" + `16`/`24`/`32` all match the static table, so the check is cosmetic. |
| 4 | MED | `readFile()` conflates "file missing" and "file empty" — both return `false`. `/proc/asound/cards` present-but-empty (no audio driver) is reported as "not readable" (wrong diagnostic) while the count check silently skips the card-count branch entirely. |
| 5 | MED | `testAudioInput()` / `testAudioOutput()`: opening a device with `O_RDONLY|O_NONBLOCK` and immediately `close()`-ing it is **not** a valid "can be opened" proof for ALSA PCM nodes (they require `snd_pcm_open` + hw_params/hw_free handshake); the test reports "device open: OK" for a raw `open(2)` that any char device would satisfy. Real capture/playback capability is never exercised. |
| 6 | LOW | `errno` is read in `testAudioInput()/testAudioOutput()` after an intervening `data.push_back(...)` call — strictly, anything between `open()` and the `errno` check can clobber it (e.g. a throwing `std::string` allocation inside `push_back`). In practice safe, but the diagnostic is unreliable. |
| 7 | LOW | `testMultiChannelAudio()` uses `minChannels` then `(void)minChannels;` — dead variable; the channel-range validation is also a static check against a hardcoded `orinMaxChannels = 16`, not the real device. |
| 8 | LOW | `orinRates` / `orinFormats` / `orinMaxChannels` are leftover **Jetson/Orin variable names** in Pi5-targeted code — misleading to a maintainer searching for Pi5-specific constants. |
| 9 | LOW | `testAudioDmaTransfers()` hardcodes `2900000.admaif` and `2930000.adma` addresses as BCM2712 peripheral addresses, but RP1 audio lives in the RP1 address space (0x1E100000 range), so the "known address" candidates are very likely never present on real hardware — the test then falls through to the generic `/sys/class/dma` listing and still passes on its own criterion. The address constants are wrong. |

## src/battery_lib/battery_lib.h

| # | Sev | Finding |
|---|-----|---------|
| 1 | LOW | No bugs; same public-test-method exposure pattern. |

## src/battery_lib/battery_lib.cpp

| # | Sev | Finding |
|---|-----|---------|
| 1 | MED | `testBatteryVoltageCurrent()`: negative battery current (the normal **discharging** state, per the linux power_supply convention) is **dropped from the output metrics** (`if (current > 0) _out.metrics["current_a"] = current;`) — a discharging battery produces no `current_a` metric, while a charging one does. Downstream consumers can't distinguish "discharging" from "no reading". |
| 2 | MED | `batParseDouble()` does not check `errno == ERANGE` (it sets `errno=0` but ignores it) → `"inf"` / `"nan"` inputs pass as a "valid" double and are stored as `capPercent`/`voltage`/`current`. Should reject non-finite values. |
| 3 | MED | `testBatteryHealth()` passes if the `status`/`health`/`capacity` files *exist and are non-empty*, regardless of content: a `status=Discharging` + `health=Unknown` is a PASS. The test name/contract implies a health verdict, but it only checks file existence. |
| 4 | LOW | `testBatteryChargeLevel()` passes on `energy_now` alone even when `capacity` is unparseable — `capPercent` stays 0, `capacity_percent` metric is suppressed. Inconsistent "passed" semantics vs. the voltage/current test which is strict about parse failure. |
| 5 | LOW | `batParseLong()` accepts `strtol` on a string with trailing garbage (e.g. `"100xyz"` → 100) — `end` is checked for *start* match only, not for full consumption. Same bug shape present wherever `strtol`/`strtod` is used with the same "start-of-string" pattern. |
## src/bluetooth_lib/bluetooth_lib.h

| # | Sev | Finding |
|---|-----|---------|
| 1 | LOW | No functional bugs; same public-method exposure pattern. |

## src/bluetooth_lib/bluetooth_lib.cpp

| # | Sev | Finding |
|---|-----|---------|
| 1 | MED | `testBluetoothPowered()`: if any `hci*` sysfs dir exists, `up = true` is set **unconditionally** — i.e. a powered-*off* adapter with an `hci0` node (the normal state after `btower off`) is reported as powered. The check is "a path exists", not a power state. |
| 2 | MED | `testBluetoothPowered()` fallback runs `hciconfig 2>&1 \| ...` via `popen` and treats **any non-empty output** as "Bluetooth is UP RUNNING" / running. If `hciconfig` is not installed, the shell prints `hciconfig: command not found` to stderr, it is captured by `2>&1`, non-empty → test **passes on a machine with no Bluetooth tooling at all**. Same latent bug in every `runCommand(...)→ non-empty ⇒ ok` pattern in this codebase. |
| 3 | MED | `testBluetoothHciAvailable()` and `testBluetoothControllerPresent()` are functionally identical (both just check the same paths) — the "HCI available" test never opens the HCI socket / checks `hciconfig` state, so it can't distinguish a dead adapter from a working one. |
| 4 | LOW | `runCommand()` discards the child exit status (`pclose` return unused) and appends unbounded output to a `std::string` with no size cap (`result += buf` in a while-loop until EOF) — a misbehaving command's multi-MB output lands verbatim in the diagnostic payload. |
| 5 | LOW | `popen`-based execution in a HAL library (vs. direct `ioctl` or `read` of sysfs) is a performance and privilege concern: the test spawns a shell per call. |
| 6 | LOW | `testBluetoothControllerPresent()`: the `found` flag is also set for `listDir("/dev/hci")` entries, but on current Linux the node is `/dev/hci0` (there is no `/dev/hci` directory), so that branch is dead — the `found=true` there can only ever fire if a directory `/dev/hci` is manually created. |

## src/camera_interfaces_lib/camera_interfaces_lib.h

| # | Sev | Finding |
|---|-----|---------|
| 1 | **HIGH** | **The first `#include "lib_base.h"` on line 2 is inside the file-level `/** ... */` Doxygen comment** — i.e. it is commented out. The file *only* compiles because a second real `#include "lib_base.h"` follows after the guard. If a generator or refactor ever removes the second one (or reorders), the header silently loses `TestBase`. This is a code-generation artifact that should be cleaned up. |
| 2 | MED | The Doxygen comments for nearly every member are the *same* boilerplate: `@brief Constructor for CameraInterfacesTesting` / `@brief Tests the X functionality` — copy-paste, not real documentation. Several comments describe the wrong member (e.g. the comment above the *destructor* also says "Constructor"). |
| 3 | LOW | `@class class CameraInterfacesTesting` — doubled keyword in the doc tag; and `@brief Class for testing camerainterfacestesting` — generator artifact (`<name>` not substituted correctly). |

## src/camera_interfaces_lib/camera_interfaces_lib.cpp

| # | Sev | Finding |
|---|-----|---------|
| 1 | **HIGH** | **Jetson/TX1 platform artifacts left in Pi5 code.** Multiple probes are for NVIDIA hardware, not Raspberry Pi 5: `host1x@13e00000/unicam@1f00110000` (TX1 device-tree), `nvisp` (TX1 ISP), `15c00000` (TX1 VI base), `/dev/rp1-vi0` (mix of Pi + Jetson naming), `/etc/nvargus`, `/var/nvidia/nvcam/settings`, `nvargus-daemon`, `bpmpClk` (TX1 BPMP clock tree), and comments that self-identify the `1f00110000` string as a "Jetson-origin Unicam hwid probe string". None of these match real Pi5 paths (RP1 unicam is under `1f00110000.csi`, ISP is `rp1-isp`, VI is `rp1-vi`) — so the tests' "hardware-specific" probes are systematically looking for non-existent device nodes on the target platform. |
| 2 | **HIGH** | `isModuleLoaded("unicam") \|\| isModuleLoaded("unicam")` — duplicated identical call (copy/paste artifact). Harmless at runtime but clearly indicates the second half of what should have been `isModuleLoaded("unicam_csi2")` or similar was never filled in. |
| 3 | MED | `testCameraClockGeneration()` Method 1 and Method 2 both point at the same path (`/sys/kernel/debug/clk`) and use almost-identical keyword lists — the "two methods" are functionally the same probe run twice, doubling the work and giving a false impression of a multi-source check. |
| 4 | MED | `testCameraClockGeneration()`: keyword matching is case-sensitive substring on the *whole* directory name (e.g. `cname.find("isp")`), which also matches unrelated clock names containing those 3–4 letters (e.g. a clock called `disp1_csi` or `visp`). This is a very loose match that will false-positive frequently. |
| 5 | MED | `testIspPipelineProcessing()`: same substring-matching bug — `base.find("isp")` matches any platform device whose name contains the letters `i-s-p` in sequence (e.g. `disp0`, `misp0`, …) → a display controller is mistaken for the camera ISP. |
| 6 | MED | `testDemosaicing()` and several "ISP feature" tests (`testLensShadingCorrection`, `testNoiseReduction`, `testEdgeEnhancement`, `testColorCorrectionMatrix`, `testGammaCorrection`) all resolve to the same decision: "an ISP-looking device node or any V4L2 device exists ⇒ PASS" — none of them exercise the named ISP feature, so a PASS on "demosaicing supported" is not evidence that demosaicing is enabled or correct. These are presence tests mis-labelled as capability/feature tests. |
| 7 | MED | `testMultipleCameraSimultaneousOperation()` claims "Raspberry Pi 5 supports up to 16 simultaneous CSI-2 camera streams" — the Pi5 datasheet supports **2** simultaneous cameras (2 Unicam instances). The stated spec is wrong by 8×, and the pass criterion (`cameraCount >= 2`) is the only part that's actually correct. |
| 8 | MED | `testRepresentativeCapture()` uses `v4l2-ctl --stream-to=/tmp/acadia_cap_<name>.raw` then encodes **the whole file as 1280×720 yuyv422** with `ffmpeg` — the frame size/format is *hardcoded* regardless of what the camera actually delivered, so the "encode" step validates a fixed geometry assumption, not the real format. If the sensor outputs a different resolution, the encode is silently malformed but still "passes" as long as a byte-non-empty mp4 is produced. |
| 9 | LOW | `readFile()` (local static) reads only the first line and returns `""` on any open error — a real value of an empty string is indistinguishable from an I/O failure, and callers treat both the same; this conflates "file missing" and "file is empty" throughout. |
| 10 | LOW | `getVideoDevices()` hardcodes a scan of `/dev/video0`..`/dev/video31` — works, but any node beyond index 31 is silently ignored; and it will also pick up non-camera V4L2 nodes (e.g. UVC webcams, tuner devices) as "CSI cameras", inflating `cameraCount` used by the multi-cam and representative-capture logic. |
| 11 | LOW | The file is ~2400 lines with heavy copy-paste: `testRaw8/10/12/14`, `testAutoExposure/WhiteBalance/Focus` etc. are near-identical blocks differing only in the `grep -iE` pattern — high maintenance cost and the source of the #2/#5 duplicate-probe bugs. |
| 12 | LOW | `runCmd()` buffers up to 4096 bytes per `fgets` call but has no total-size cap across iterations — a very verbose `v4l2-ctl` or `media-ctl` output is appended unboundedly and lands in the `data[]` diagnostic vector. |
