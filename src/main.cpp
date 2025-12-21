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

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "network/quic_api.hpp"
#include "network/quic_certificate.hpp"
#include "network/quic_config_manager.hpp"
#include "network/quic_connection.hpp"
#include "network/quic_server.hpp"

namespace quicflow {
namespace network {

// Signal handler for graceful shutdown.
void SignalHandler(int signal) {
  if (QuicServer::GetInstance().is_listening()) {
    std::cout << "\n[QuicFlow] Received signal " << signal
              << ", shutting down server..." << std::endl;
    QuicServer::GetInstance().Stop();
  }
}

}  // namespace network
}  // namespace quicflow

int main() {
  using namespace quicflow;
  using namespace quicflow::network;

  // Create and start the QUIC server.
  constexpr uint16_t kServerPort = 4433;
  QuicServer& server = QuicServer::GetInstance();
  QUIC_STATUS status = server.InitQuicServer(kServerPort);
  if (QUIC_FAILED(status)) {
    std::cout << "[QuicFlow] Server initialization failed" << std::endl;

    return EXIT_FAILURE;
  }

  // Set up connection callback.
  // Why: When a new connection is accepted, we want to log it and potentially
  //      create a connection handler. In a full implementation, this callback
  //      would create a QuicConnection wrapper and register stream callbacks.

  // Register signal handlers for graceful shutdown.
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
  int count = 0;
  while (server.is_listening()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    if (count++ % 10 == 0) {
      std::cout <<".";
    }

  }

  std::cout << "[QuicFlow] Server stopped" << std::endl;
  return EXIT_SUCCESS;
}


