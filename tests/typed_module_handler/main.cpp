// test_typed_module_handler — regression for #50.
//
// Verifies that TypedModuleHandler::RunSingleShot stamps a
// "module_config_unset" reason key on every TestResult when the client
// sent an empty google.protobuf.Any for module_config (and that it
// still runs all tests, rather than failing outright).
//
// Behavioural contract being tested:
//   - An empty Any => run all tests (legacy behaviour preserved).
//   - Every result must carry reason["module_config_unset"] == "true".
//   - A wrong-type Any => INVALID_ARGUMENT (also tested).

#include "server/typed_module_handler.h"
#include "cpu_lib/cpu_lib.h"
#include "common.pb.h"
#include "modules/cpu.pb.h"

#include <cstdio>
#include <cstdlib>
#include <string>

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
    Handler h("cpu", 1, "src/cpu_lib/cpu_lib.h");

    // ------------------------------------------------------------------
    // Case 1: empty module_config Any => run all, stamp diagnostic.
    // ------------------------------------------------------------------
    {
        google::protobuf::Any        empty_any;
        hal::common::CommonConfig   common;
        hal::common::ResultEnvelope env;
        grpc::Status st = h.RunSingleShot(nullptr, empty_any, common, &env);
        EXPECT(st.ok(), "empty Any returns OK status");
        EXPECT(env.payload().type_url().size() > 0,
               "envelope payload is a packed Result");

        hal::modules::cpu::Result rp;
        EXPECT(env.payload().UnpackTo(&rp), "payload unpacks to cpu::Result");
        EXPECT(rp.results_size() > 0, "at least one TestResult returned");

        if (rp.results_size() > 0) {
            bool all_stamped = true;
            for (const auto& r : rp.results()) {
                auto it = r.output().reason().find("module_config_unset");
                if (it == r.output().reason().end() || it->second != "true") {
                    all_stamped = false;
                    break;
                }
            }
            EXPECT(all_stamped,
                   "every TestResult has reason[module_config_unset]=true");
        }
    }

    // ------------------------------------------------------------------
    // Case 2: wrong-type Any (pack gpu.Config) => INVALID_ARGUMENT.
    // ------------------------------------------------------------------
    {
        hal::modules::cpu::Config bogus;  // use a non-cpu module to force a type URL
        // Re-pack as a fake "type" by abusing the type_url field of Any.
        // Easier: just pack a hal::common::CommonConfig into the slot.
        google::protobuf::Any wrong_any;
        hal::common::CommonConfig cc;
        wrong_any.PackFrom(cc);

        hal::common::CommonConfig   common;
        hal::common::ResultEnvelope env;
        grpc::Status st = h.RunSingleShot(nullptr, wrong_any, common, &env);
        EXPECT(!st.ok(), "wrong-type Any returns non-OK");
        EXPECT(st.error_code() == grpc::StatusCode::INVALID_ARGUMENT,
               "wrong-type Any returns INVALID_ARGUMENT");
    }

    if (g_failures != 0) {
        std::fprintf(stderr, "\n%d failure(s)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "\nall checks passed\n");
    return 0;
}
