#pragma once

// typed_module_handler.h — template implementation of IModuleHandler.
//
// TypedModuleHandler<HalClass, ConfigProto, ResultProto> handles the four test
// modes for any HAL module by instantiating HalClass, unpacking the Any config,
// running the appropriate TestBase API, and packing the results back.
//
// All 35 module Config messages share the same structure:
//   message Config { repeated string test_names = 1; }
// and all Result messages share:
//   message Result { repeated hal.common.TestResultEntry test_results = 1; }
//
// Usage (in module_registry_all.cpp):
//   registry->Register("cpu",
//       std::make_unique<TypedModuleHandler<CpuTesting,
//                                          hal::modules::cpu::Config,
//                                          hal::modules::cpu::Result>>(
//           "cpu", 1, "src/cpu_lib/cpu_lib.h"));

#include "server/hal_service_impl.h"
#include "common.pb.h"
#include "lib_base.h"

#include <google/protobuf/timestamp.pb.h>
#include <grpcpp/grpcpp.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace hal {
namespace server {

// ============================================================
// Proto conversion helpers (used from TypedModuleHandler)
// ============================================================

inline hal::common::TestOutput ToProto(const ::TestOutput& o) {
    hal::common::TestOutput p;
    p.set_passed(o.passed);
    for (const auto& d : o.data) p.add_data(d);
    return p;
}

inline hal::common::TestResultEntry ToProto(const ::TestResult& r) {
    hal::common::TestResultEntry p;
    p.set_name(r.name);
    *p.mutable_output() = ToProto(r.output);
    p.set_duration_sec(r.duration.count());
    return p;
}

inline google::protobuf::Timestamp NowTimestamp() {
    google::protobuf::Timestamp ts;
    auto now = std::chrono::system_clock::now().time_since_epoch();
    ts.set_seconds(
        static_cast<int64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(now).count()));
    ts.set_nanos(static_cast<int32_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count()
        % 1'000'000'000));
    return ts;
}

inline void FillEnvelope(hal::common::ResultEnvelope*  env,
                          const std::string&             module_id,
                          int                            version,
                          hal::common::ExecutionMode     mode,
                          hal::common::TestStatus        status,
                          const std::string&             error_msg = {}) {
    env->set_module_id(module_id);
    env->set_module_version(std::to_string(version) + ".0.0");
    *env->mutable_timestamp() = NowTimestamp();
    env->set_mode(mode);
    env->set_status(status);
    if (!error_msg.empty()) env->set_error_message(error_msg);
}

inline hal::common::TestStatus ComputeStatus(
    const std::vector<::TestResult>& results) {
    if (results.empty()) return hal::common::TestStatus::ERROR;
    bool all_pass = true, any_pass = false;
    for (const auto& r : results) {
        if (r.output.passed) any_pass = true;
        else                 all_pass = false;
    }
    if (all_pass) return hal::common::TestStatus::PASS;
    if (any_pass) return hal::common::TestStatus::PARTIAL;
    return hal::common::TestStatus::FAIL;
}

// ============================================================
// TypedModuleHandler<HalClass, ConfigProto, ResultProto>
// ============================================================

template<typename HalClass, typename ConfigProto, typename ResultProto>
class TypedModuleHandler : public IModuleHandler {
  public:
    TypedModuleHandler(std::string module_id, int version, std::string source_path)
        : module_id_(std::move(module_id))
        , version_(version)
        , source_path_(std::move(source_path)) {}

    // ----------------------------------------------------------------
    // RunSingleShot
    // ----------------------------------------------------------------
    grpc::Status RunSingleShot(grpc::ServerContext*              /*ctx*/,
                                const google::protobuf::Any&     module_config,
                                const hal::common::CommonConfig& /*common*/,
                                hal::common::ResultEnvelope*     out) override {
        ConfigProto cfg;
        TryUnpack(module_config, cfg);

        HalClass hal_module;
        auto results = RunTests(hal_module, cfg);

        ResultProto rp;
        for (const auto& r : results) *rp.add_test_results() = ToProto(r);

        FillEnvelope(out, module_id_, version_,
                     hal::common::ExecutionMode::SINGLE_SHOT,
                     ComputeStatus(results));
        out->mutable_payload()->PackFrom(rp);
        return grpc::Status::OK;
    }

    // ----------------------------------------------------------------
    // StartLongTerm
    // ----------------------------------------------------------------
    grpc::Status StartLongTerm(
        grpc::ServerContext*                              ctx,
        const google::protobuf::Any&                     module_config,
        const hal::common::CommonConfig&                 common,
        grpc::ServerWriter<hal::common::ResultEnvelope>* writer) override {
        ConfigProto cfg;
        TryUnpack(module_config, cfg);

        int      interval_ms = std::max(common.interval_ms(), 100);
        int64_t  dur_ms      = common.run_forever() ? 0LL : common.duration_ms();

        auto hal_module = std::make_shared<HalClass>();
        StoreActive(hal_module);

        std::mutex write_mu;
        hal_module->runLongTerm(
            interval_ms, dur_ms,
            [&](const std::vector<::TestResult>& results, int /*cycle*/) {
                hal::common::ResultEnvelope env;
                ResultProto rp;
                for (const auto& r : results) *rp.add_test_results() = ToProto(r);
                FillEnvelope(&env, module_id_, version_,
                             hal::common::ExecutionMode::LONG_TERM,
                             ComputeStatus(results));
                env.mutable_payload()->PackFrom(rp);
                std::lock_guard<std::mutex> wlk(write_mu);
                writer->Write(env);
            });

        WaitOrCancel(ctx, *hal_module);
        ClearActive();
        return grpc::Status::OK;
    }

    // ----------------------------------------------------------------
    // StartMonitoring
    // ----------------------------------------------------------------
    grpc::Status StartMonitoring(
        grpc::ServerContext*                              ctx,
        const google::protobuf::Any&                     module_config,
        const hal::common::CommonConfig&                 common,
        const hal::common::MonitoringParams&             monitoring,
        grpc::ServerWriter<hal::common::ResultEnvelope>* writer) override {
        ConfigProto cfg;
        TryUnpack(module_config, cfg);

        int     interval_ms = std::max(common.interval_ms(), 100);
        int64_t dur_ms      = common.run_forever() ? 0LL : common.duration_ms();

        // Build MonitorItems from the requested names.
        // The server-side reader callable returns the name as a placeholder;
        // real deployments should inject a monitor item registry.
        std::vector<::MonitorItem> monitor_items;
        for (const auto& name : monitoring.monitor_item_names()) {
            ::MonitorItem item;
            item.name   = name;
            item.reader = [name]() -> std::string { return name + ":0"; };
            monitor_items.push_back(std::move(item));
        }

        auto hal_module = std::make_shared<HalClass>();
        StoreActive(hal_module);

        std::mutex write_mu;
        hal_module->runMonitoringTest(
            interval_ms, dur_ms, monitor_items,
            [&](const ::MonitoringSnapshot& snap) {
                hal::common::ResultEnvelope   env;
                hal::common::MonitoringSnapshot proto_snap;
                proto_snap.set_cycle(snap.cycle);
                *proto_snap.mutable_timestamp() = NowTimestamp();
                for (const auto& r : snap.testResults)
                    *proto_snap.add_test_results() = ToProto(r);
                for (const auto& kv : snap.monitorData) {
                    auto* pair = proto_snap.add_monitor_data();
                    pair->set_key(kv.first);
                    pair->set_value(kv.second);
                }
                FillEnvelope(&env, module_id_, version_,
                             hal::common::ExecutionMode::MONITORING,
                             ComputeStatus(snap.testResults));
                env.mutable_payload()->PackFrom(proto_snap);
                std::lock_guard<std::mutex> wlk(write_mu);
                writer->Write(env);
            });

        WaitOrCancel(ctx, *hal_module);
        ClearActive();
        return grpc::Status::OK;
    }

    // ----------------------------------------------------------------
    // StartEventBased
    // ----------------------------------------------------------------
    grpc::Status StartEventBased(
        grpc::ServerContext*                              ctx,
        const google::protobuf::Any&                     module_config,
        const hal::common::CommonConfig&                 common,
        const hal::common::EventCondition&               condition,
        grpc::ServerWriter<hal::common::ResultEnvelope>* writer) override {
        ConfigProto cfg;
        TryUnpack(module_config, cfg);

        int     poll_ms = std::max(common.poll_interval_ms(), 100);
        int64_t max_ms  = common.run_forever() ? 0LL : common.duration_ms();

        ::EventCriteria criteria;
        criteria.description = condition.expression().empty()
            ? ("test=" + condition.test_name() +
               " expected_pass=" + (condition.expected_pass() ? "true" : "false"))
            : condition.expression();

        std::string tname    = condition.test_name();
        bool        exp_pass = condition.expected_pass();
        criteria.predicate   = [tname, exp_pass](const std::vector<::TestResult>& res) {
            if (tname.empty()) return false;
            for (const auto& r : res)
                if (r.name == tname) return r.output.passed == exp_pass;
            return false;
        };

        auto hal_module = std::make_shared<HalClass>();
        StoreActive(hal_module);

        std::mutex write_mu;
        hal_module->runEventTest(
            poll_ms, criteria,
            [&](const std::vector<::TestResult>& results) {
                hal::common::ResultEnvelope env;
                ResultProto rp;
                for (const auto& r : results) *rp.add_test_results() = ToProto(r);
                FillEnvelope(&env, module_id_, version_,
                             hal::common::ExecutionMode::EVENT_BASED,
                             ComputeStatus(results));
                env.mutable_payload()->PackFrom(rp);
                std::lock_guard<std::mutex> wlk(write_mu);
                writer->Write(env);
            },
            max_ms);

        WaitOrCancel(ctx, *hal_module);
        ClearActive();
        return grpc::Status::OK;
    }

    // ----------------------------------------------------------------
    // Stop
    // ----------------------------------------------------------------
    void Stop() override {
        std::lock_guard<std::mutex> lk(active_mu_);
        if (active_module_) active_module_->stop();
    }

    // ----------------------------------------------------------------
    // GetModuleInfo
    // ----------------------------------------------------------------
    hal::ModuleInfo GetModuleInfo() const override {
        hal::ModuleInfo info;
        info.set_id(module_id_);
        info.set_proto_package("hal.modules." + module_id_);
        info.set_config_type("hal.modules." + module_id_ + ".Config");
        info.set_result_type("hal.modules." + module_id_ + ".Result");
        info.set_version(version_);
        info.set_status("active");
        info.set_source_path(source_path_);
        return info;
    }

  private:
    // Run selected tests, or all tests when test_names is empty.
    static std::vector<::TestResult> RunTests(HalClass& m, const ConfigProto& cfg) {
        if (cfg.test_names_size() == 0)
            return m.runSelectedTests(m.getAvailableTests());
        std::vector<std::string> names(cfg.test_names().begin(), cfg.test_names().end());
        return m.runSelectedTests(names);
    }

    // Unpack google.protobuf.Any → ConfigProto; silently skips if the Any is empty.
    static void TryUnpack(const google::protobuf::Any& any, ConfigProto& cfg) {
        if (!any.type_url().empty()) any.UnpackTo(&cfg);
    }

    // Poll until the HAL background thread exits, or the RPC is cancelled.
    static void WaitOrCancel(grpc::ServerContext* ctx, ::TestBase& m) {
        while (m.isRunning()) {
            if (ctx && ctx->IsCancelled()) { m.stop(); return; }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    void StoreActive(std::shared_ptr<::TestBase> module) {
        std::lock_guard<std::mutex> lk(active_mu_);
        active_module_ = std::move(module);
    }

    void ClearActive() {
        std::lock_guard<std::mutex> lk(active_mu_);
        active_module_.reset();
    }

    std::string              module_id_;
    int                      version_;
    std::string              source_path_;

    std::mutex               active_mu_;
    std::shared_ptr<::TestBase> active_module_; // non-null while a background test is running
};

} // namespace server
} // namespace hal
