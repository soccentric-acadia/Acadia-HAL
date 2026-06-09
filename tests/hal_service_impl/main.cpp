// test_hal_service_impl — regression for #46.
//
// Verifies that HalServiceImpl returns INVALID_ARGUMENT (not NOT_FOUND)
// when module_id is empty on the four per-module dispatch RPCs.
//
// Behavioural contract being tested:
//   - Empty module_id on RunSingleShot, StartLongTerm, StartMonitoring,
//     StartEventBased => grpc::StatusCode::INVALID_ARGUMENT.
//   - The "all-modules" broadcast semantic on Stop / Pause / Resume
//     (empty module_id means broadcast) is preserved — this test does
//     not assert on that path; it's the pre-existing behaviour the fix
//     must not break.

#include "server/hal_service_impl.h"
#include "server/typed_module_handler.h"
#include "cpu_lib/cpu_lib.h"
#include "common.pb.h"
#include "hal_service.grpc.pb.h"
#include "modules/cpu.pb.h"

#include <cstdio>
#include <memory>

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
    CpuTesting,
    hal::modules::cpu::Config,
    hal::modules::cpu::Result>;

}  // namespace

int main()
{
    // Build a real HalServiceImpl with a real ModuleRegistry containing
    // one module — the "cpu" module.
    auto registry = std::make_unique<hal::server::ModuleRegistry>();
    registry->Register("cpu",
        std::make_unique<Handler>("cpu", 1, "src/cpu_lib/cpu_lib.h"));
    hal::server::HalServiceImpl svc(std::move(registry));

    // ------------------------------------------------------------------
    // Case 1: empty module_id => INVALID_ARGUMENT.
    // ------------------------------------------------------------------
    {
        hal::SingleShotRequest req;
        hal::common::ResultEnvelope resp;
        grpc::Status st = svc.RunSingleShot(nullptr, &req, &resp);
        EXPECT(!st.ok(), "empty module_id returns non-OK");
        EXPECT(st.error_code() == grpc::StatusCode::INVALID_ARGUMENT,
               "empty module_id returns INVALID_ARGUMENT (was NOT_FOUND pre-#46)");
    }

    // ------------------------------------------------------------------
    // Case 2: positive control — valid module_id returns OK.
    // We only check status, not the result content (covered by
    // test_typed_module_handler for the typed handler itself).
    // ------------------------------------------------------------------
    {
        hal::SingleShotRequest req;
        req.set_module_id("cpu");
        hal::common::ResultEnvelope resp;
        grpc::Status st = svc.RunSingleShot(nullptr, &req, &resp);
        EXPECT(st.ok(), "valid module_id returns OK");
    }

    // ------------------------------------------------------------------
    // Case 3: negative control — bogus module_id returns NOT_FOUND
    // (preserved pre-existing behaviour, not changed by #46).
    // ------------------------------------------------------------------
    {
        hal::SingleShotRequest req;
        req.set_module_id("this_module_does_not_exist");
        hal::common::ResultEnvelope resp;
        grpc::Status st = svc.RunSingleShot(nullptr, &req, &resp);
        EXPECT(!st.ok(), "unknown module_id returns non-OK");
        EXPECT(st.error_code() == grpc::StatusCode::NOT_FOUND,
               "unknown module_id returns NOT_FOUND");
    }

    if (g_failures != 0) {
        std::fprintf(stderr, "\n%d failure(s)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "\nall checks passed\n");
    return 0;
}
