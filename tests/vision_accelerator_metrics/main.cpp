// test_vision_accelerator_metrics — regression for #63.
//
// Verifies that vision_accelerator_lib reports its clearly-measurable
// values as typed `metrics` (hal::Value, not opaque strings) so
// downstream consumers (Web UI charts, threshold alerts) can rely on
// type rather than string parsing.
//
// Per req.md §2 rule 1: numeric/bool data must NOT be stringified.
// The original refactor in Phase 5 placed all 572 ambiguous `data`
// entries under `reason["detail_N"]` (positional strings). This test
// pins down the typed-metric surface for vision_accelerator and
// provides a template that other modules can adopt opportunistically
// (per the local CLAUDE.md "Do" entry for #61/#63).
//
// Tests asserted:
//   - testVectorExecutionUnits → metrics["neon_memcpy_ns"] is an int64
//     AND metrics["neon_memcpy_gbps"] is a double (the headline
//     NEON throughput numbers must be typed).
//   - testDmaBandwidthTesting → metrics["dma_memcpy_ms"] is a double
//     AND metrics["dma_memcpy_gbps"] is a double.
//   - testDmaLatencyTesting → metrics["dma_proxy_avg_latency_ns"] is
//     an int64 (the threshold (< 100 µs) is a numeric comparison).
//   - For all the above, the original `reason["detail_N"]` strings are
//     still present (so the human-readable summary in the runner is
//     preserved — the change is purely additive).

#include "server/typed_module_handler.h"
#include "vision_accelerator_lib/vision_accelerator_lib.h"
#include "common.pb.h"
#include "modules/vision_accelerator.pb.h"

#include <cstdio>
#include <string>
#include <variant>

namespace {

int g_failures = 0;

#define EXPECT(cond, msg) do {                                                \
    if (!(cond)) {                                                            \
        std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, msg);    \
        ++g_failures;                                                         \
    } else {                                                                  \
        std::fprintf(stderr, "ok   %s\n", msg);                              \
    }                                                                         \
} while (0)

using Handler = hal::server::TypedModuleHandler<
    VisionAcceleratorTesting,
    hal::modules::vision_accelerator::Config,
    hal::modules::vision_accelerator::Result>;

// Find a single TestResult by test_id; returns nullptr if not found.
const hal::common::TestResult* findResult(
    const hal::modules::vision_accelerator::Result& rp,
    const std::string& test_id)
{
    for (const auto& r : rp.results()) {
        if (r.test_id() == test_id) return &r;
    }
    return nullptr;
}

}  // namespace

int main()
{
    Handler h("vision_accelerator", 1, "src/vision_accelerator_lib/vision_accelerator_lib.h");

    // Run a SINGLE_SHOT with empty config; expect one envelope with all tests.
    google::protobuf::Any        empty_any;
    hal::common::CommonConfig   common;
    hal::common::ResultEnvelope env;
    grpc::Status st = h.RunSingleShot(nullptr, empty_any, common, &env);
    EXPECT(st.ok(), "SINGLE_SHOT returns OK");
    if (!st.ok()) {
        std::fprintf(stderr, "  grpc::Status error_message: %s\n",
                     st.error_message().c_str());
        return 1;
    }

    hal::modules::vision_accelerator::Result rp;
    EXPECT(env.payload().UnpackTo(&rp), "payload unpacks to vision_accelerator::Result");
    if (!env.payload().UnpackTo(&rp)) return 1;

    // ------------------------------------------------------------------
    // testVectorExecutionUnits → typed memcpy latency + throughput.
    // ------------------------------------------------------------------
    {
        const auto* r = findResult(rp, "testVectorExecutionUnits");
        EXPECT(r != nullptr, "found testVectorExecutionUnits in payload");
        if (r) {
            auto it_ns = r->output().metrics().find("neon_memcpy_ns");
            EXPECT(it_ns != r->output().metrics().end(),
                   "metrics[neon_memcpy_ns] is present");
            if (it_ns != r->output().metrics().end()) {
                EXPECT(it_ns->second.value_case() ==
                           hal::common::TypedValue::kIntValue,
                       "metrics[neon_memcpy_ns] is an int64 (typed, not stringified)");
                if (it_ns->second.value_case() == hal::common::TypedValue::kIntValue) {
                    EXPECT(it_ns->second.int_value() > 0,
                           "metrics[neon_memcpy_ns] is a positive number");
                }
            }
            auto it_gbps = r->output().metrics().find("neon_memcpy_gbps");
            EXPECT(it_gbps != r->output().metrics().end(),
                   "metrics[neon_memcpy_gbps] is present");
            if (it_gbps != r->output().metrics().end()) {
                EXPECT(it_gbps->second.value_case() ==
                           hal::common::TypedValue::kDoubleValue,
                       "metrics[neon_memcpy_gbps] is a double (typed, not stringified)");
            }
            // The original reason[detail_N] surface is still preserved
            // (additive change — human-readable summary intact).
            EXPECT(r->output().reason().count("detail_0") == 1,
                   "reason[detail_0] still present (preserved for runner output)");
        }
    }

    // ------------------------------------------------------------------
    // testDmaBandwidthTesting → typed DMA-proxy latency + throughput.
    // ------------------------------------------------------------------
    {
        const auto* r = findResult(rp, "testDmaBandwidthTesting");
        EXPECT(r != nullptr, "found testDmaBandwidthTesting in payload");
        if (r) {
            auto it_ms = r->output().metrics().find("dma_memcpy_ms");
            EXPECT(it_ms != r->output().metrics().end(),
                   "metrics[dma_memcpy_ms] is present");
            if (it_ms != r->output().metrics().end()) {
                EXPECT(it_ms->second.value_case() ==
                           hal::common::TypedValue::kDoubleValue,
                       "metrics[dma_memcpy_ms] is a double (typed, not stringified)");
            }
            auto it_gbps = r->output().metrics().find("dma_memcpy_gbps");
            EXPECT(it_gbps != r->output().metrics().end(),
                   "metrics[dma_memcpy_gbps] is present");
            if (it_gbps != r->output().metrics().end()) {
                EXPECT(it_gbps->second.value_case() ==
                           hal::common::TypedValue::kDoubleValue,
                       "metrics[dma_memcpy_gbps] is a double (typed, not stringified)");
            }
        }
    }

    // ------------------------------------------------------------------
    // testDmaLatencyTesting → typed avg latency ns (threshold field).
    // ------------------------------------------------------------------
    {
        const auto* r = findResult(rp, "testDmaLatencyTesting");
        EXPECT(r != nullptr, "found testDmaLatencyTesting in payload");
        if (r) {
            auto it = r->output().metrics().find("dma_proxy_avg_latency_ns");
            EXPECT(it != r->output().metrics().end(),
                   "metrics[dma_proxy_avg_latency_ns] is present");
            if (it != r->output().metrics().end()) {
                EXPECT(it->second.value_case() ==
                           hal::common::TypedValue::kIntValue,
                       "metrics[dma_proxy_avg_latency_ns] is an int64 (typed, not stringified)");
            }
        }
    }

    if (g_failures != 0) {
        std::fprintf(stderr, "\n%d failure(s)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "\nall checks passed\n");
    return 0;
}
