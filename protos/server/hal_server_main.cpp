// hal_server_main.cpp — standalone HAL gRPC server entry point.
//
// Usage:
//   hal_server [<address>]
//   hal_server 0.0.0.0:50051      # default
//   hal_server 127.0.0.1:9090

#include "server/hal_service_impl.h"
#include "server/module_registry_all.h"

#include <grpcpp/grpcpp.h>
#include <grpcpp/security/server_credentials.h>

#include <csignal>
#include <iostream>
#include <memory>
#include <string>

static std::unique_ptr<grpc::Server> g_server;

static void OnSignal(int /*sig*/) {
    if (g_server) {
        std::cout << "\n[hal_server] shutting down...\n";
        g_server->Shutdown();
    }
}

int main(int argc, char* argv[]) {
    const std::string address = (argc > 1) ? argv[1] : "0.0.0.0:50051";

    // Build the service with all 35 HAL modules registered.
    auto registry = hal::server::CreateDefaultRegistry();
    hal::server::HalServiceImpl service(std::move(registry));

    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    // Allow large messages — test data blobs can be sizable.
    builder.SetMaxReceiveMessageSize(64 * 1024 * 1024);  // 64 MB
    builder.SetMaxSendMessageSize(64 * 1024 * 1024);

    g_server = builder.BuildAndStart();
    if (!g_server) {
        std::cerr << "[hal_server] failed to start on " << address << "\n";
        return 1;
    }

    std::cout << "[hal_server] listening on " << address << "\n";
    std::signal(SIGINT,  OnSignal);
    std::signal(SIGTERM, OnSignal);

    g_server->Wait();
    return 0;
}
