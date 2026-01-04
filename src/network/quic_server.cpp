#include "network/quic_server.hpp"

#include <cstring>
#include <iostream>

#include "manager/connection_manager.hpp"
#include "network/quic_config_manager.hpp"
#include "network/quic_connection.hpp"

namespace quicflow {
namespace network {

QuicServer::QuicServer() {
  std::cout << "QuicServer Instance Created" << std::endl;
}


const QUIC_API_TABLE* QuicServer::api() {
  if (config_ == nullptr) {
    return nullptr;
  }
  return config_->api();
}

QUIC_STATUS QuicServer::InitQuicServer(uint16_t port) {

  port_ = port;
  is_listening_ = false;
  listener_ = nullptr;

  // Create QUIC configuration with HTTP/3 and WebTransport ALPN.
  // Why: We support multiple ALPN protocols for backward compatibility and
  //      future extensibility (WebTransport for real-time chat).
  //QuicConfigManager config(api, {"h3", "h3-29", "webtransport"});
  std::shared_ptr<QuicConfigManager> config(new QuicConfigManager());

  if (config->InitializeConfig() == false) {
    std::cerr << "[QuicFlow] Failed to initialize QUIC configuration: "
              << config->error_message() << std::endl;
    return QUIC_STATUS_INTERNAL_ERROR;
  }
  config_ = config;

  if (config_ == nullptr || !config_->is_valid()) {
    error_message_ = "QuicConfigManager is not valid";

    return QUIC_STATUS_INTERNAL_ERROR;
  }

  return QUIC_STATUS_SUCCESS;
}

QuicServer::QuicServer(QuicServer&& other) noexcept
    : config_(other.config_),
      listener_(other.listener_),
      port_(other.port_),
      is_listening_(other.is_listening_),
      error_message_(std::move(other.error_message_)){
  // Reset source object to prevent double cleanup.
  other.config_ = nullptr;
  other.listener_ = nullptr;
  other.port_ = 0;
  other.is_listening_ = false;
  other.error_message_.clear();
}

QuicServer& QuicServer::operator=(QuicServer&& other) noexcept {
  if (this != &other) {
    Cleanup();

    config_ = other.config_;
    listener_ = other.listener_;

    port_ = other.port_;
    is_listening_ = other.is_listening_;
    error_message_ = std::move(other.error_message_);

    // Reset source object.
    other.config_ = nullptr;
    other.listener_ = nullptr;
    other.port_ = 0;
    other.is_listening_ = false;
    other.error_message_.clear();
  }
  return *this;
}

QuicServer::~QuicServer() { Cleanup(); }

bool QuicServer::Start() {
  if (is_listening_) {
    error_message_ = "Server is already listening";
    return false;
  }

  if (config_ == nullptr) {
    error_message_ = "QuicApi or QuicConfigManager is not available";
    return false;
  }

  // Open the listener with our callback function.
  // Why: ListenerCallback is a static method, so we pass 'this' as context
  //      to access instance members.
  auto api = config_->api();
  auto config= config_->configuration();
  auto registration = config_->registration();

  QUIC_STATUS status = api->ListenerOpen(
      registration,  // Registration handle
      ServerListenerCallback,   // Static callback function
      this,               // Context (QuicServer*)
      &listener_);

  if (QUIC_FAILED(status)) {
    error_message_ = "Failed to open listener: status " + std::to_string(status);
    return false;
  }

  // Start listening on the specified port.
  // Why: QUIC_ADDRESS_FAMILY_UNSPEC allows both IPv4 and IPv6.
  //      We bind to INADDR_ANY (0.0.0.0) to accept connections from any interface.
  QUIC_ADDR address{};
  QuicAddrSetFamily(&address, QUIC_ADDRESS_FAMILY_UNSPEC);
  QuicAddrSetPort(&address, port_);

  auto buffers = config_->alpn_buffers();

  // ListenerStart signature: (HQUIC, const QUIC_BUFFER*, uint32_t, const QUIC_ADDR*)
  // Why: We pass nullptr for ALPN buffers to use the configuration's ALPN.
  //      AlpnBufferCount must be 0 when AlpnBuffers is nullptr.
  status = api->ListenerStart(listener_, &buffers[0], 1, &address);

  if (QUIC_FAILED(status)) {
    error_message_ = "Failed to start listener on port " + std::to_string(port_) +
                     ": status " + std::to_string(status);
    Cleanup();
    return false;
  }

  is_listening_ = true;
  std::cout << "[QuicServer] Started listening on UDP port " << port_ << std::endl;
  return true;
}

void QuicServer::Stop() noexcept {
  if (!is_listening_) {
    return;
  }

  if (listener_ != nullptr) {
    auto api = config_->api();
    api->ListenerStop(listener_);
  }

  Cleanup();
  std::cout << "[QuicServer] Stopped listening on port " << port_ << std::endl;
}
QUIC_STATUS QUIC_API QuicServer::ServerListenerCallback(HQUIC listener,
                                                  void* context,
                                                  QUIC_LISTENER_EVENT* event) {
  // Cast context back to QuicServer instance.
  // Why: Static method cannot access instance members, so we use the context
  //      pointer that was passed during ListenerOpen.
  QuicServer* server = static_cast<QuicServer*>(context);
  if (server == nullptr) {
    return QUIC_STATUS_INVALID_PARAMETER;
  }

  auto config = server->config();
  if (config == nullptr) {
    return QUIC_STATUS_INVALID_PARAMETER;
  }

  std::cout << "ServerListenerCallback called (" << event->Type << ")" <<std::endl;

  auto api = config->api();
  switch (event->Type) {
    case QUIC_LISTENER_EVENT_NEW_CONNECTION: {
      // A new connection is attempting to connect.
      // Why: We accept the connection and assign our configuration to it.
      //      The connection handle is then passed to the user callback.
      HQUIC hConnection= event->NEW_CONNECTION.Connection;

      std::cout << "QUIC_LISTENER_EVENT_NEW_CONNECTION called" << std::endl;

      // Accept the connection with our configuration.
      QUIC_STATUS status = api->ConnectionSetConfiguration(hConnection, config->configuration());

      if (QUIC_FAILED(status)) {
        std::cerr << "[QuicServer] Failed to set connection configuration: "
                  << status << std::endl;
        return status;
      }

      // Invoke user callback if set.
      // Why: User callback allows application code to handle the new connection
      //      (e.g., create a QuicConnection wrapper, register stream callbacks, etc.).
      try {
        auto newConnection = std::make_shared<QuicConnection>(hConnection);

        auto status = newConnection->InitConnection(server);
        if (QUIC_FAILED(status)) {
          std::cerr << "[QuicServer] Failed to init connection: " << status << std::endl;
          return QUIC_STATUS_INTERNAL_ERROR;
        }

        manager::ConnectionManager::GetInstance().OnNewConnection(newConnection);

      } catch (const std::exception& e) {
        std::cerr << "[QuicServer] Exception in connection callback: " << e.what()
                  << std::endl;
        return QUIC_STATUS_INTERNAL_ERROR;
      }

      return QUIC_STATUS_SUCCESS;
    }

    case QUIC_LISTENER_EVENT_STOP_COMPLETE:
      // Listener has stopped (can be used for cleanup if needed).
      std::cout << "[QuicServer] Listener stop completed" << std::endl;
      return QUIC_STATUS_SUCCESS;

    default:
      // Ignore other event types for now.
      return QUIC_STATUS_SUCCESS;
  }
}

void QuicServer::Cleanup() noexcept {
  if (listener_ != nullptr) {
    //api_->ListenerClose(listener_);
    listener_ = nullptr;
  }

  is_listening_ = false;
}

}  // namespace network
}  // namespace quicflow

