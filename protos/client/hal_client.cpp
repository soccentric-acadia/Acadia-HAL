#include "client/hal_client.h"

#include <chrono>
#include <cstdlib>

namespace hal {
namespace client {

namespace {

// Resolve a per-RPC deadline.
//
// Two sources, in order:
//   1. The HAL_CLIENT_RPC_TIMEOUT_MS env var (milliseconds; applies to
//      every RPC, including the streaming ones).
//   2. The `default_ms` argument chosen by the call site (e.g. 5s for
//      Stop/Pause/Resume, 30s for RunSingleShot, INT_MAX-ish for the
//      long-running streaming RPCs).
//
// We deliberately do NOT also read a per-RPC env var — the surface is
// intentionally tiny. The read happens once per RPC; getenv is async-
// signal-safe per POSIX so this is fine even if the call lands during
// a SIGINT cleanup.
std::chrono::system_clock::time_point ComputeDeadline(
    std::chrono::milliseconds default_ms) {
    const char* env = std::getenv("HAL_CLIENT_RPC_TIMEOUT_MS");
    std::chrono::milliseconds ms = default_ms;
    if (env && *env) {
        try {
            long long v = std::stoll(env);
            if (v > 0) ms = std::chrono::milliseconds(v);
        } catch (...) {
            // Bad env var → fall back to default. Not worth warning
            // every call; users get a one-shot hint from the README.
        }
    }
    return std::chrono::system_clock::now() + ms;
}

} // namespace

HalClient::HalClient(std::shared_ptr<grpc::Channel> channel)
    : stub_(hal::HalService::NewStub(std::move(channel))) {}

grpc::Status HalClient::RunSingleShot(const std::string&               module_id,
                                       const google::protobuf::Message& module_config,
                                       const hal::common::CommonConfig& common,
                                       hal::common::ResultEnvelope*     out) {
    hal::SingleShotRequest req;
    req.set_module_id(module_id);
    *req.mutable_common() = common;
    req.mutable_module_config()->PackFrom(module_config);

    grpc::ClientContext ctx;
    // 30s default for a one-shot — most modules finish in << 1s but some
    // (e.g. storage soak, reliability soak) can take longer. The env
    // var override lets CI bump this without rebuilding.
    ctx.set_deadline(ComputeDeadline(std::chrono::seconds(30)));
    return stub_->RunSingleShot(&ctx, req, out);
}

grpc::Status HalClient::StartLongTerm(const std::string&               module_id,
                                       const google::protobuf::Message& module_config,
                                       const hal::common::CommonConfig& common,
                                       ResultCallback                   on_result) {
    hal::LongTermRequest req;
    req.set_module_id(module_id);
    *req.mutable_common() = common;
    req.mutable_module_config()->PackFrom(module_config);

    grpc::ClientContext ctx;
    // Streaming RPC — use a generous 1h default; clients that want a
    // tighter cap set HAL_CLIENT_RPC_TIMEOUT_MS.
    ctx.set_deadline(ComputeDeadline(std::chrono::hours(1)));
    auto reader = stub_->StartLongTerm(&ctx, req);

    hal::common::ResultEnvelope env;
    while (reader->Read(&env)) {
        if (on_result) on_result(env);
    }
    return reader->Finish();
}

grpc::Status HalClient::StartMonitoring(const std::string&                   module_id,
                                         const google::protobuf::Message&     module_config,
                                         const hal::common::CommonConfig&     common,
                                         const hal::common::MonitoringParams& monitoring,
                                         ResultCallback                        on_result) {
    hal::MonitoringRequest req;
    req.set_module_id(module_id);
    *req.mutable_common() = common;
    *req.mutable_monitoring() = monitoring;
    req.mutable_module_config()->PackFrom(module_config);

    grpc::ClientContext ctx;
    ctx.set_deadline(ComputeDeadline(std::chrono::hours(1)));
    auto reader = stub_->StartMonitoring(&ctx, req);

    hal::common::ResultEnvelope env;
    while (reader->Read(&env)) {
        if (on_result) on_result(env);
    }
    return reader->Finish();
}

grpc::Status HalClient::StartEventBased(const std::string&                  module_id,
                                         const google::protobuf::Message&    module_config,
                                         const hal::common::CommonConfig&    common,
                                         const hal::common::EventCondition&  condition,
                                         ResultCallback                      on_result) {
    hal::EventBasedRequest req;
    req.set_module_id(module_id);
    *req.mutable_common() = common;
    *req.mutable_condition() = condition;
    req.mutable_module_config()->PackFrom(module_config);

    grpc::ClientContext ctx;
    ctx.set_deadline(ComputeDeadline(std::chrono::hours(1)));
    auto reader = stub_->StartEventBased(&ctx, req);

    hal::common::ResultEnvelope env;
    while (reader->Read(&env)) {
        if (on_result) on_result(env);
    }
    return reader->Finish();
}

grpc::Status HalClient::Stop(const std::string& module_id,
                              const std::string& session_id) {
    hal::StopRequest req;
    req.set_module_id(module_id);
    req.set_session_id(session_id);

    grpc::ClientContext ctx;
    // 5s default — Stop is a control RPC and should respond fast. A
    // stuck server will DEADLINE_EXCEED instead of hanging the caller
    // forever, which is the whole point of the deadline.
    ctx.set_deadline(ComputeDeadline(std::chrono::seconds(5)));
    hal::StopResponse resp;
    return stub_->Stop(&ctx, req, &resp);
}

grpc::Status HalClient::Pause(const std::string& module_id,
                               const std::string& session_id) {
    hal::PauseRequest req;
    req.set_module_id(module_id);
    req.set_session_id(session_id);

    grpc::ClientContext ctx;
    ctx.set_deadline(ComputeDeadline(std::chrono::seconds(5)));
    hal::PauseResponse resp;
    return stub_->Pause(&ctx, req, &resp);
}

grpc::Status HalClient::Resume(const std::string& module_id,
                                const std::string& session_id) {
    hal::ResumeRequest req;
    req.set_module_id(module_id);
    req.set_session_id(session_id);

    grpc::ClientContext ctx;
    ctx.set_deadline(ComputeDeadline(std::chrono::seconds(5)));
    hal::ResumeResponse resp;
    return stub_->Resume(&ctx, req, &resp);
}

grpc::Status HalClient::ListModules(std::vector<hal::ModuleInfo>* modules) {
    hal::ListModulesRequest req;
    grpc::ClientContext ctx;
    // 5s default — ListModules is a one-shot control RPC.
    ctx.set_deadline(ComputeDeadline(std::chrono::seconds(5)));
    hal::ListModulesResponse resp;

    auto status = stub_->ListModules(&ctx, req, &resp);
    if (status.ok())
        modules->assign(resp.modules().begin(), resp.modules().end());
    return status;
}

} // namespace client
} // namespace hal
