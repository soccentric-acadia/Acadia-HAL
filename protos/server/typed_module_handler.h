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
//   message Result { repeated hal.common.TestResult results = 2; }
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
#include "test_runner.h"

#include <google/protobuf/duration.pb.h>
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
// Proto conversion helpers — new canonical shape
// ============================================================

inline hal::common::TestStatus ToProtoStatus(::hal::Status s) {
    switch (s) {
        case ::hal::Status::PASS:    return hal::common::TestStatus::TEST_STATUS_PASS;
        case ::hal::Status::FAIL:    return hal::common::TestStatus::TEST_STATUS_FAIL;
        case ::hal::Status::ERROR:   return hal::common::TestStatus::TEST_STATUS_ERROR;
        case ::hal::Status::TIMEOUT: return hal::common::TestStatus::TEST_STATUS_TIMEOUT;
        case ::hal::Status::SKIPPED: return hal::common::TestStatus::TEST_STATUS_SKIPPED;
        case ::hal::Status::ABORTED: return hal::common::TestStatus::TEST_STATUS_ABORTED;
        default:                     return hal::common::TestStatus::TEST_STATUS_ERROR;
    }
}

inline hal::common::TestSeverity ToProtoSeverity(::hal::Severity sv) {
    switch (sv) {
        case ::hal::Severity::INFO:     return hal::common::TestSeverity::TEST_SEVERITY_INFO;
        case ::hal::Severity::WARNING:  return hal::common::TestSeverity::TEST_SEVERITY_WARNING;
        case ::hal::Severity::ERROR:    return hal::common::TestSeverity::TEST_SEVERITY_ERROR;
        case ::hal::Severity::CRITICAL: return hal::common::TestSeverity::TEST_SEVERITY_CRITICAL;
        default:                        return hal::common::TestSeverity::TEST_SEVERITY_INFO;
    }
}

inline hal::common::TypedValue ToProtoTypedValue(const ::hal::Value& v) {
    hal::common::TypedValue tv;
    if (const bool* b = std::get_if<bool>(&v)) {
        tv.set_bool_value(*b);
    } else if (const int64_t* i = std::get_if<int64_t>(&v)) {
        tv.set_int_value(*i);
    } else if (const double* d = std::get_if<double>(&v)) {
        tv.set_double_value(*d);
    } else if (const std::string* s = std::get_if<std::string>(&v)) {
        tv.set_string_value(*s);
    } else {
        // [SERIALIZER-FALLBACK] Unknown variant index — encoding as empty string.
        tv.set_string_value("");
    }
    return tv;
}

inline hal::common::TestOutput ToProtoTestOutput(const ::hal::TestOutput& o) {
    hal::common::TestOutput p;
    p.set_status(ToProtoStatus(o.status));
    p.set_severity(ToProtoSeverity(o.severity));
    for (const auto& [k, v] : o.reason)     (*p.mutable_reason())[k]     = v;
    for (const auto& [k, v] : o.metrics)    (*p.mutable_metrics())[k]    = ToProtoTypedValue(v);
    for (const auto& [k, v] : o.monitoring) (*p.mutable_monitoring())[k] = ToProtoTypedValue(v);
    for (const auto& [k, v] : o.event)      (*p.mutable_event())[k]      = ToProtoTypedValue(v);
    if (!o.error_message.empty()) p.set_error_message(o.error_message);
    return p;
}

// Forward declaration for recursive subtests.
inline hal::common::TestResult ToProtoTestResult(const ::hal::TestResult& r);

inline hal::common::TestResult ToProtoTestResult(const ::hal::TestResult& r) {
    hal::common::TestResult p;
    p.set_module_id(r.module_id);
    p.set_test_id(r.test_id);
    p.set_name(r.name);
    p.set_sequence(r.sequence);
    // timestamp
    {
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            r.timestamp.time_since_epoch());
        google::protobuf::Timestamp ts;
        ts.set_seconds(static_cast<int64_t>(ns.count() / 1'000'000'000LL));
        ts.set_nanos(static_cast<int32_t>(ns.count() % 1'000'000'000LL));
        *p.mutable_timestamp() = ts;
    }
    // duration
    {
        google::protobuf::Duration dur;
        dur.set_seconds(static_cast<int64_t>(r.duration.count() / 1'000'000'000LL));
        dur.set_nanos(static_cast<int32_t>(r.duration.count() % 1'000'000'000LL));
        *p.mutable_duration() = dur;
    }
    *p.mutable_output() = ToProtoTestOutput(r.output);
    for (const auto& sub : r.subtests)
        *p.add_subtests() = ToProtoTestResult(sub);
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
    if (results.empty()) return hal::common::TestStatus::TEST_STATUS_ERROR;
    bool all_pass = true;
    for (const auto& r : results) {
        if (r.output.status != ::hal::Status::PASS) { all_pass = false; break; }
    }
    if (all_pass) return hal::common::TestStatus::TEST_STATUS_PASS;
    // Some passes + some fails: per common.proto, PARTIAL (enum value 6) is
    // a deprecated envelope-level aggregate that must no longer be
    // produced. The run as a whole did not pass, so the envelope status
    // is FAIL. Per-test results in the payload still carry their own
    // individual statuses (PASS / FAIL / ERROR / ...), so a consumer can
    // always reconstruct the breakdown from envelope.payload().
    return hal::common::TestStatus::TEST_STATUS_FAIL;
}

// Stamp module_id, timestamp (if not already set), and sequence onto a results
// vector. Called by RunTests() after the HAL module's tests have executed.
// If `config_unset` is true, also stamps a diagnostic reason key on every
// result so a client that forgot to send module_config is visible in the
// audit trail (Fix #50).
inline void StampResults(std::vector<::TestResult>& results,
                          const std::string&          module_id,
                          uint64_t                    sequence,
                          bool                        config_unset = false) {
    auto now = std::chrono::system_clock::now();
    for (auto& r : results) {
        r.module_id = module_id;
        r.sequence  = sequence;
        if (r.timestamp == std::chrono::system_clock::time_point{})
            r.timestamp = now;
        if (r.test_id.empty()) r.test_id = r.name;
        if (config_unset)
            r.output.reason["module_config_unset"] = "true";
    }
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
        const auto up = TryUnpack(module_config, cfg);
        if (up == UnpackResult::kTypeMismatch) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "module_config type_url mismatch: " +
                                module_config.type_url());
        }
        const bool config_unset = (up == UnpackResult::kEmpty);

        HalClass hal_module;
        auto results = RunAndStamp(hal_module, cfg, /*sequence=*/0, config_unset);

        ResultProto rp;
        for (const auto& r : results) *rp.add_results() = ToProtoTestResult(r);

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
        const auto up = TryUnpack(module_config, cfg);
        if (up == UnpackResult::kTypeMismatch) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "module_config type_url mismatch: " +
                                module_config.type_url());
        }
        const bool config_unset = (up == UnpackResult::kEmpty);

        int      interval_ms = std::max(common.interval_ms(), 100);
        int64_t  dur_ms      = common.run_forever() ? 0LL : common.duration_ms();

        auto hal_module = std::make_shared<HalClass>();
        StoreActive(hal_module);

        std::mutex write_mu;
        uint64_t   seq = 0;
        std::atomic<bool> stream_ok{true};  // Task 59: track broken/cancelled stream
        hal_module->runLongTerm(
            interval_ms, dur_ms,
            [&](const std::vector<::TestResult>& raw_results, int /*cycle*/) {
                if (!stream_ok.load()) return;  // stop producing after a failed write
                auto results = raw_results;
                StampResults(results, module_id_, seq++, config_unset);
                hal::common::ResultEnvelope env;
                ResultProto rp;
                for (const auto& r : results) *rp.add_results() = ToProtoTestResult(r);
                FillEnvelope(&env, module_id_, version_,
                             hal::common::ExecutionMode::LONG_TERM,
                             ComputeStatus(results));
                env.mutable_payload()->PackFrom(rp);
                std::lock_guard<std::mutex> wlk(write_mu);
                if (!writer->Write(env)) {
                    // Broken/cancelled stream: stop promptly, preserve state.
                    stream_ok.store(false);
                    hal_module->stop();
                }
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
        const auto up = TryUnpack(module_config, cfg);
        if (up == UnpackResult::kTypeMismatch) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "module_config type_url mismatch: " +
                                module_config.type_url());
        }
        const bool config_unset = (up == UnpackResult::kEmpty);

        int     interval_ms = std::max(common.interval_ms(), 100);
        int64_t dur_ms      = common.run_forever() ? 0LL : common.duration_ms();

        std::vector<::MonitorItem> monitor_items;
        // Task 57: resolve requested monitor names against the REAL module
        // readers (the RPi5 monitor item set) instead of fabricating "name:0".
        // Unknown/unsupported names get an explicit "unsupported" reader so a
        // consumer sees a distinct status, never a fabricated value.
        auto real_readers = ::trPi5MonitorItems();
        for (const auto& name : monitoring.monitor_item_names()) {
            ::MonitorItem item;
            item.name = name;
            const auto it =
                std::find_if(real_readers.begin(), real_readers.end(),
                             [&](const ::MonitorItem& r) { return r.name == name; });
            if (it != real_readers.end()) {
                item.reader = it->reader;  // real sysfs-backed reader
            } else {
                item.reader = [name]() -> std::string {
                    return name + ":unsupported";
                };
            }
            monitor_items.push_back(std::move(item));
        }

        auto hal_module = std::make_shared<HalClass>();
        StoreActive(hal_module);

        std::mutex write_mu;
        uint64_t   seq = 0;
        std::atomic<bool> stream_ok{true};  // Task 59
        hal_module->runMonitoringTest(
            interval_ms, dur_ms, monitor_items,
            [&](const ::MonitoringSnapshot& snap) {
                if (!stream_ok.load()) return;
                auto results = snap.testResults;
                // Stamp monitoring data into each result's monitoring map.
                for (auto& r : results) {
                    for (const auto& kv : snap.monitorData) {
                        r.output.monitoring[kv.first] =
                            ::hal::Value{std::string(kv.second)};
                    }
                }
                StampResults(results, module_id_, seq++, config_unset);

                hal::common::ResultEnvelope   env;
                hal::common::MonitoringSnapshot proto_snap;
                proto_snap.set_cycle(snap.cycle);
                *proto_snap.mutable_timestamp() = NowTimestamp();
                for (const auto& r : results)
                    *proto_snap.add_test_results() = ToProtoTestResult(r);
                for (const auto& kv : snap.monitorData) {
                    auto* pair = proto_snap.add_monitor_data();
                    pair->set_key(kv.first);
                    pair->set_value(kv.second);
                }
                FillEnvelope(&env, module_id_, version_,
                             hal::common::ExecutionMode::MONITORING,
                             ComputeStatus(results));
                env.mutable_payload()->PackFrom(proto_snap);
                std::lock_guard<std::mutex> wlk(write_mu);
                if (!writer->Write(env)) {
                    stream_ok.store(false);
                    hal_module->stop();
                }
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
        const auto up = TryUnpack(module_config, cfg);
        if (up == UnpackResult::kTypeMismatch) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                "module_config type_url mismatch: " +
                                module_config.type_url());
        }
        const bool config_unset = (up == UnpackResult::kEmpty);

        int     poll_ms = std::max(common.poll_interval_ms(), 100);
        int64_t max_ms  = common.run_forever() ? 0LL : common.duration_ms();

        ::EventCriteria criteria;
        criteria.description = condition.expression().empty()
            ? ("test=" + condition.test_name() +
               " expected_pass=" + (condition.expected_pass() ? "true" : "false"))
            : condition.expression();

        std::string tname    = condition.test_name();
        bool        exp_pass = condition.expected_pass();
        std::string expr     = condition.expression();
        criteria.predicate   = [tname, exp_pass, expr](const std::vector<::TestResult>& res) {
            // Task 58: freeform expression form ("cpu_temp_c > 80"). Resolve the
            // metric to a real monitor reader's current value; reject invalid
            // expressions by never firing.
            if (!expr.empty()) {
                return ::trEvaluateEventExpression(
                    expr,
                    [](const std::string& metric) -> double {
                        for (const auto& mi : ::trPi5MonitorItems()) {
                            if (mi.name == metric) {
                                std::string v = mi.reader();
                                try { return std::stod(v); }
                                catch (...) { return -1.0; }
                            }
                        }
                        return -1.0;  // unknown metric -> no value
                    });
            }
            if (tname.empty()) return false;
            for (const auto& r : res)
                if (r.name == tname)
                    return (r.output.status == ::hal::Status::PASS) == exp_pass;
            return false;
        };

        auto hal_module = std::make_shared<HalClass>();
        StoreActive(hal_module);

        std::mutex write_mu;
        uint64_t   seq = 0;
        std::atomic<bool> stream_ok{true};  // Task 59
        hal_module->runEventTest(
            poll_ms, criteria,
            [&](const std::vector<::TestResult>& raw_results) {
                if (!stream_ok.load()) return;
                auto results = raw_results;
                // Populate event map with trigger info.
                for (auto& r : results) {
                    r.output.event["trigger_test"]     = ::hal::Value{tname};
                    r.output.event["expected_pass"]    = ::hal::Value{exp_pass};
                    r.output.event["observed_pass"]    =
                        ::hal::Value{r.output.status == ::hal::Status::PASS};
                }
                StampResults(results, module_id_, seq++, config_unset);
                hal::common::ResultEnvelope env;
                ResultProto rp;
                for (const auto& r : results) *rp.add_results() = ToProtoTestResult(r);
                FillEnvelope(&env, module_id_, version_,
                             hal::common::ExecutionMode::EVENT_BASED,
                             ComputeStatus(results));
                env.mutable_payload()->PackFrom(rp);
                std::lock_guard<std::mutex> wlk(write_mu);
                if (!writer->Write(env)) {
                    stream_ok.store(false);
                    hal_module->stop();
                }
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
    // Pause
    // ----------------------------------------------------------------
    void Pause() override {
        std::lock_guard<std::mutex> lk(active_mu_);
        if (active_module_) active_module_->pause();
    }

    // ----------------------------------------------------------------
    // Resume
    // ----------------------------------------------------------------
    void Resume() override {
        std::lock_guard<std::mutex> lk(active_mu_);
        if (active_module_) active_module_->resume();
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
    // Run selected tests, stamp results, and return them.
    std::vector<::TestResult> RunAndStamp(HalClass& m, const ConfigProto& cfg,
                                           uint64_t sequence,
                                           bool     config_unset = false) {
        std::vector<::TestResult> results;
        if (cfg.test_names_size() == 0)
            results = m.runSelectedTests(m.getAvailableTests());
        else {
            std::vector<std::string> names(cfg.test_names().begin(),
                                           cfg.test_names().end());
            results = m.runSelectedTests(names);
        }
        StampResults(results, module_id_, sequence, config_unset);
        return results;
    }

    // Unpack google.protobuf.Any → ConfigProto; classifies the outcome so
    // callers can stamp a diagnostic or fail with INVALID_ARGUMENT.
    //   kEmpty        — Any was unset (client sent no module_config).
    //   kTypeMismatch — Any.type_url() set but UnpackTo failed.
    //   kOk           — UnpackTo succeeded.
    enum class UnpackResult { kEmpty, kTypeMismatch, kOk };
    static UnpackResult TryUnpack(const google::protobuf::Any& any,
                                  ConfigProto&                  cfg) {
        if (any.type_url().empty()) return UnpackResult::kEmpty;
        if (!any.UnpackTo(&cfg))    return UnpackResult::kTypeMismatch;
        return UnpackResult::kOk;
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
