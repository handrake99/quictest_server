// QuicFlow-CPP - QUIC Server Entry Point
// ---------------------------------------
// This file demonstrates the integration of QuicApi, QuicConfigManager, and
// QuicServer to create a functional QUIC server that listens for client connections.
//
// Why this design?
//  - We separate concerns: API initialization, configuration, and server lifecycle
//    are handled by dedicated classes (QuicApi, QuicConfigManager, QuicServer).
//  - RAII ensures proper resource cleanup even in error paths.
//  - The server can be started/stopped explicitly, making it suitable for
//    both long-running services and test scenarios.

#include <cstdlib>
#include <csignal>
#include <iostream>
#include <thread>
#include <chrono>

#include "network/quic_api.hpp"
#include "network/quic_config_manager.hpp"
#include "network/quic_server.hpp"
#include "network/quic_certificate.hpp"

namespace quicflow {
namespace network {

// Global server pointer for signal handling.
// Why: Signal handlers (SIGINT, SIGTERM) need access to the server instance
//      to perform graceful shutdown. Using a global pointer is a simple
//      solution for this use case.
QuicServer* g_server = nullptr;

// Signal handler for graceful shutdown.
void SignalHandler(int signal) {
  if (g_server != nullptr) {
    std::cout << "\n[QuicFlow] Received signal " << signal
              << ", shutting down server..." << std::endl;
    g_server->Stop();
  }
}

}  // namespace network
}  // namespace quicflow

int main() {
  using namespace quicflow;
  using namespace quicflow::network;

  // Initialize MsQuic API.
  QuicApi api{};
  if (!api.is_available()) {
    std::cerr << "[QuicFlow] MsQuic API not available at runtime. "
              << "The container may be missing libmsquic or QUICFLOW_HAS_MSQUIC "
              << "was not defined at compile time.\n";
    return EXIT_FAILURE;
  }

  // Create QUIC configuration with HTTP/3 and WebTransport ALPN.
  // Why: We support multiple ALPN protocols for backward compatibility and
  //      future extensibility (WebTransport for real-time chat).
  QuicConfigManager config(api, {"h3", "h3-29", "webtransport"});
  if (!config.is_valid()) {
    std::cerr << "[QuicFlow] Failed to create QUIC configuration: "
              << config.error_message() << std::endl;
    return EXIT_FAILURE;
  }

  // Load certificate from files.
  // Why: QUIC/TLS requires a certificate. We load the certificate and key
  //      from files in the certificate/ directory for production use.
#ifdef QUICFLOW_HAS_MSQUIC
  const std::string cert_file = "certificate/server.crt";
  const std::string key_file = "certificate/server.key";

  auto cred_config = LoadCertificateFromFiles(cert_file, key_file);
  if (cred_config.Type == QUIC_CREDENTIAL_TYPE_NONE) {
    std::cerr << "[QuicFlow] Failed to load certificate from files" << std::endl;
    return EXIT_FAILURE;
  }

  if (!config.set_credential(cred_config)) {
    std::cerr << "[QuicFlow] Failed to set certificate: "
              << config.error_message() << std::endl;
    return EXIT_FAILURE;
  }
#endif

  // Create and start the QUIC server.
  constexpr uint16_t kServerPort = 4433;
  QuicServer server(api, config, kServerPort);

  // Set up connection callback.
  // Why: When a new connection is accepted, we want to log it and potentially
  //      create a connection handler. In a full implementation, this callback
  //      would create a QuicConnection wrapper and register stream callbacks.
#ifdef QUICFLOW_HAS_MSQUIC
  server.SetConnectionCallback([](HQUIC connection, void* /*context*/) {
    std::cout << "[QuicFlow] New connection accepted: " << connection << std::endl;
    // TODO: Create QuicConnection wrapper and register stream callbacks.
    // TODO: In Phase 2, enqueue connection handling to Boost.Asio strand.
  });
#else
  server.SetConnectionCallback([](void* /*connection*/, void* /*context*/) {
    std::cout << "[QuicFlow] New connection accepted (MsQuic not available)"
              << std::endl;
  });
#endif

  // Register signal handlers for graceful shutdown.
  g_server = &server;
  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  // Start the server.
  if (!server.Start()) {
    std::cerr << "[QuicFlow] Failed to start server: " << server.error_message()
              << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "[QuicFlow] Server is running on UDP port " << kServerPort
            << std::endl;
  std::cout << "[QuicFlow] Press Ctrl+C to stop the server" << std::endl;

  // Main event loop: keep the server running until stopped.
  // Why: MsQuic uses an event-driven model. The server will process events
  //      in the background, but we need to keep the main thread alive.
  //      In Phase 2, this will be replaced with Boost.Asio's io_context::run().
  while (server.is_listening()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout <<".";
  }

  std::cout << "[QuicFlow] Server stopped" << std::endl;
  return EXIT_SUCCESS;
}


