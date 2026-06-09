// hal_server_main.cpp — standalone HAL gRPC server entry point.
//
// Usage:
//   hal_server [<address>]
//   hal_server 0.0.0.0:50051      # default
//   hal_server 127.0.0.1:9090
//
// TLS configuration (environment variables):
//   HAL_TLS_CERT   — path to PEM-encoded server certificate
//   HAL_TLS_KEY    — path to PEM-encoded server private key
//   HAL_TLS_CA     — path to PEM-encoded CA certificate (enables mutual TLS)
//
// When HAL_TLS_CERT and HAL_TLS_KEY are set, the server uses TLS.
// When HAL_TLS_CA is also set, client certificates are required (mTLS).
// Without these variables the server falls back to insecure mode (development only).

#include "server/hal_service_impl.h"
#include "server/module_registry_all.h"

#include <grpcpp/grpcpp.h>
#include <grpcpp/security/server_credentials.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

static std::unique_ptr<grpc::Server> g_server;

// Async-signal-safe shutdown signalling.
//
// The previous implementation called g_server->Shutdown() and std::cout
// directly inside the SIGINT/SIGTERM handler. Neither is on the POSIX
// async-signal-safe list, and gRPC explicitly warns against invoking
// Server::Shutdown from a signal handler. The fix is to write a single
// std::atomic<bool> flag in the handler (the only safe operation besides
// _exit / SIG_DFL) and have the main thread observe it and call Shutdown
// from a context where it's safe.
static std::atomic<bool> g_shutdown_requested{false};

static void OnSignal(int sig) {
    // Async-signal-safe body: a single atomic store. Everything else
    // (gRPC Shutdown, std::cout) is driven from the main thread.
    //
    // The first signal asks the server to shut down gracefully. A second
    // signal restores the default disposition so the operator can still
    // hard-kill a process whose gRPC stack is wedged in the middle of
    // Shutdown(). std::signal() is async-signal-safe.
    if (g_shutdown_requested.load(std::memory_order_acquire)) {
        std::signal(sig, SIG_DFL);
        // Re-raise so the default handler actually runs. raise() is
        // async-signal-safe per POSIX.
        std::raise(sig);
        return;
    }
    g_shutdown_requested.store(true, std::memory_order_release);
}

// Read a file into a string. Returns empty on error.
static std::string readFile(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::shared_ptr<grpc::ServerCredentials> buildCredentials() {
    const char* cert_path = std::getenv("HAL_TLS_CERT");
    const char* key_path  = std::getenv("HAL_TLS_KEY");
    const char* ca_path   = std::getenv("HAL_TLS_CA");

    if (cert_path && key_path) {
        std::string cert = readFile(cert_path);
        std::string key  = readFile(key_path);
        if (cert.empty() || key.empty()) {
            std::cerr << "[hal_server] ERROR: TLS cert/key files not readable. Aborting.\n";
            std::exit(1);
        }
        grpc::SslServerCredentialsOptions ssl_opts(
            ca_path ? GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY
                    : GRPC_SSL_DONT_REQUEST_CLIENT_CERTIFICATE);
        ssl_opts.pem_key_cert_pairs.push_back({key, cert});
        if (ca_path) {
            std::string ca = readFile(ca_path);
            if (ca.empty()) {
                std::cerr << "[hal_server] ERROR: TLS CA file not readable. Aborting.\n";
                std::exit(1);
            }
            ssl_opts.pem_root_certs = ca;
        }
        std::cout << "[hal_server] TLS enabled"
                  << (ca_path ? " (mTLS)" : "") << "\n";
        return grpc::SslServerCredentials(ssl_opts);
    }

    std::cerr << "[hal_server] WARNING: TLS not configured — running insecure mode. "
              << "Set HAL_TLS_CERT and HAL_TLS_KEY for production deployments.\n";
    return grpc::InsecureServerCredentials();
}

int main(int argc, char* argv[]) {
    const std::string address = (argc > 1) ? argv[1] : "0.0.0.0:50051";

    // Build the service with all 35 HAL modules registered.
    auto registry = hal::server::CreateDefaultRegistry();
    hal::server::HalServiceImpl service(std::move(registry));

    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, buildCredentials());
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

    // Drive gRPC shutdown from a dedicated watcher thread. This keeps the
    // gRPC API and std::cout calls out of the signal handler, where they
    // are not async-signal-safe.
    std::thread shutdown_watcher([&]() {
        while (!g_shutdown_requested.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (g_server) {
            std::cout << "\n[hal_server] shutting down...\n";
            g_server->Shutdown();
        }
    });

    g_server->Wait();
    // Ensure the watcher exits even if g_server->Wait() returned for a
    // reason other than the signal (e.g. the server builder failed).
    g_shutdown_requested.store(true, std::memory_order_release);
    if (shutdown_watcher.joinable()) shutdown_watcher.join();
    return 0;
}
