# Module Migration: vision_accelerator_lib

**Source:** `src/vision_accelerator_lib/vision_accelerator_lib.cpp`

**Changes applied:** 6 (5 test methods reclassified + the ambiguous_log row
removed after #63)

## Migration summary

Per req.md §2 rule 1, clearly-measurable values were extracted from the
positional `reason["detail_N"]` strings into typed `metrics` entries. The
original human-readable strings are still preserved in `reason` (the
typed-metric surface is additive — the runner output and audit trail are
unchanged).

### Reclassified measurements (issue #63)

| Test | Metric key | Type | Source value |
|---|---|---|---|
| `testDualVectorProcessingUnits` | `cpu_core_count` | `int64` | `grep -c 'processor' /proc/cpuinfo` |
| `testDualVectorProcessingUnits` | `v3d_clock_hz` | `int64` | `vcgencmd measure_clock v3d` (Hz) |
| `testScalarExecutionUnits` | `cpu0_cur_freq_khz` | `int64` | sysfs `scaling_cur_freq` (kHz) |
| `testScalarExecutionUnits` | `cpu0_max_freq_khz` | `int64` | sysfs `cpuinfo_max_freq` (kHz) |
| `testScalarExecutionUnits` | `cpu_max_freq_rated_mhz` | `int64` | `RPI5_CPU_MAX_FREQ_MHZ` constant |
| `testVectorExecutionUnits` | `neon_memcpy_ns` | `int64` | measured memcpy latency (ns) |
| `testVectorExecutionUnits` | `neon_memcpy_gbps` | `double` | measured memcpy throughput (GB/s) |
| `testDmaEngine1Functionality` | `bcm_dma_module_count` | `int64` | `grep -c` on `/proc/modules` |
| `testDmaBandwidthTesting` | `dma_memcpy_ms` | `double` | 128 MB DMA-proxy latency (ms) |
| `testDmaBandwidthTesting` | `dma_memcpy_gbps` | `double` | 128 MB DMA-proxy throughput (GB/s) |
| `testDmaLatencyTesting` | `dma_proxy_avg_latency_ns` | `int64` | 4 KB DMA-proxy avg latency (ns) |

The remaining `reason["detail_N"]` strings (human-readable `[INFO]`,
`[PASS]`, `[WARN]` log lines) stay as strings per req.md §2 rule 4
(free-form explanation).

### Regression test

`tests/vision_accelerator_metrics/main.cpp` (added in #63) wires
through `TypedModuleHandler` and asserts:

- `metrics["neon_memcpy_ns"]` is an `int64` and positive.
- `metrics["neon_memcpy_gbps"]` is a `double`.
- `metrics["dma_memcpy_ms"]` and `metrics["dma_memcpy_gbps"]` are `double`.
- `metrics["dma_proxy_avg_latency_ns"]` is an `int64`.
- `reason["detail_0"]` is still present (additive change — the
  human-readable summary is preserved).

All 17 assertions in the new test pass. The remaining test entries
(`testVectorCoresInEachVpu`, `testVliwSlots`, `testDmaEngine2Functionality`,
`test5dimensionalAddressing`, `test16HardwareChannelsPerEngine`,
`testComputerVisionAlgorithms`, `testImageProcessingPipelines`,
`testFeatureDetection`, `testOpticalFlow`, `testStereoDisparity`,
`testPvaPowerConsumption`) keep their `reason["detail_N"]` strings
because they carry explanatory/diagnostic content rather than typed
measurements. They can be revisited opportunistically as the module is
touched for other reasons.
