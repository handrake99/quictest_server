#include "network/quic_server.hpp"

#include <cstring>
#include <iostream>

#include "network/quic_api.hpp"
#include "network/quic_config_manager.hpp"

namespace quicflow {
namespace network {

QuicServer::QuicServer(const QuicApi& api,
                       const QuicConfigManager& config,
                       uint16_t port)
#ifdef QUICFLOW_HAS_MSQUIC
    : api_(api.native()),
      config_(&config),
      listener_(nullptr),
#else
    : api_(nullptr),
      config_(nullptr),
      listener_(nullptr),
#endif
      port_(port),
      is_listening_(false) {
  if (api_ == nullptr) {
    error_message_ = "QuicApi is not available";
    return;
  }

  if (config_ == nullptr || !config_->is_valid()) {
    error_message_ = "QuicConfigManager is not valid";
    return;
  }
}

QuicServer::QuicServer(QuicServer&& other) noexcept
#ifdef QUICFLOW_HAS_MSQUIC
    : api_(other.api_),
      config_(other.config_),
      listener_(other.listener_),
#else
    : api_(other.api_),
      config_(other.config_),
      listener_(other.listener_),
#endif
      port_(other.port_),
      is_listening_(other.is_listening_),
      error_message_(std::move(other.error_message_)),
      connection_callback_(std::move(other.connection_callback_)) {
  // Reset source object to prevent double cleanup.
  other.api_ = nullptr;
  other.config_ = nullptr;
  other.listener_ = nullptr;
  other.port_ = 0;
  other.is_listening_ = false;
  other.error_message_.clear();
}

QuicServer& QuicServer::operator=(QuicServer&& other) noexcept {
  if (this != &other) {
    Cleanup();

#ifdef QUICFLOW_HAS_MSQUIC
    api_ = other.api_;
    config_ = other.config_;
    listener_ = other.listener_;
#else
    api_ = other.api_;
    config_ = other.config_;
    listener_ = other.listener_;
#endif

    port_ = other.port_;
    is_listening_ = other.is_listening_;
    error_message_ = std::move(other.error_message_);
    connection_callback_ = std::move(other.connection_callback_);

    // Reset source object.
    other.api_ = nullptr;
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

#ifdef QUICFLOW_HAS_MSQUIC
  if (api_ == nullptr || config_ == nullptr) {
    error_message_ = "QuicApi or QuicConfigManager is not available";
    return false;
  }

  // Open the listener with our callback function.
  // Why: ListenerCallback is a static method, so we pass 'this' as context
  //      to access instance members.
  // Note: ListenerOpen requires a REGISTRATION handle, not a configuration handle.
  HQUIC registration_handle = config_->registration();
  std::cerr << "[DEBUG][F] Before ListenerOpen: registration_handle=" << registration_handle 
            << ", config_valid=" << (config_->is_valid() ? "true" : "false") << std::endl;

  QUIC_STATUS status = api_->ListenerOpen(
      registration_handle,  // Registration handle (not configuration!)
      ListenerCallback,   // Static callback function
      this,               // Context (QuicServer*)
      &listener_);

  std::cerr << "[DEBUG][F] After ListenerOpen: status=" << status 
            << " (0x" << std::hex << status << std::dec << "), listener_ptr=" 
            << (listener_ ? "non-null" : "null") << std::endl;

  if (status != QUIC_STATUS_SUCCESS) {
    error_message_ = "Failed to open listener: status " + std::to_string(status);
    std::cerr << "[ERROR] ListenerOpen failed with status " << status << std::endl;
    return false;
  }

  // Start listening on the specified port.
  // Why: QUIC_ADDRESS_FAMILY_UNSPEC allows both IPv4 and IPv6.
  //      We bind to INADDR_ANY (0.0.0.0) to accept connections from any interface.
  QUIC_ADDR address{};
  QuicAddrSetFamily(&address, QUIC_ADDRESS_FAMILY_UNSPEC);
  QuicAddrSetPort(&address, port_);

  // ListenerStart signature: (HQUIC, const QUIC_BUFFER*, uint32_t, const QUIC_ADDR*)
  // Why: ListenerStart requires ALPN buffers to be explicitly passed.
  //      We cannot pass nullptr with count 0 - we must pass the actual ALPN buffers.
  std::cerr << "[DEBUG][F] Before ListenerStart: listener=" << listener_ 
            << ", port=" << port_ << std::endl;
  
  // Get ALPN buffers from configuration
  // Note: We need to reconstruct ALPN buffers or get them from config.
  // For now, let's try with a single ALPN buffer for "h3"
  static uint8_t alpn_data[] = {'h', '3'};  // Static storage for ALPN string
  QUIC_BUFFER alpn_buffer{};
  alpn_buffer.Length = sizeof(alpn_data);
  alpn_buffer.Buffer = alpn_data;
  
  status = api_->ListenerStart(listener_, &alpn_buffer, 1, &address);
  
  std::cerr << "[DEBUG][F] After ListenerStart: status=" << status 
            << " (0x" << std::hex << status << std::dec << ")" << std::endl;

  if (status != QUIC_STATUS_SUCCESS) {
    error_message_ = "Failed to start listener on port " + std::to_string(port_) +
                     ": status " + std::to_string(status);
    Cleanup();
    return false;
  }

  is_listening_ = true;
  std::cout << "[QuicServer] Started listening on UDP port " << port_ << std::endl;
  return true;
#else
  error_message_ = "QUICFLOW_HAS_MSQUIC is not defined at compile time";
  return false;
#endif
}

void QuicServer::Stop() noexcept {
  if (!is_listening_) {
    return;
  }

#ifdef QUICFLOW_HAS_MSQUIC
  if (listener_ != nullptr && api_ != nullptr) {
    api_->ListenerStop(listener_);
  }
#endif

  Cleanup();
  std::cout << "[QuicServer] Stopped listening on port " << port_ << std::endl;
}

void QuicServer::SetConnectionCallback(ConnectionCallback callback) {
  connection_callback_ = std::move(callback);
}

#ifdef QUICFLOW_HAS_MSQUIC
QUIC_STATUS QUIC_API QuicServer::ListenerCallback(HQUIC listener,
                                                  void* context,
                                                  QUIC_LISTENER_EVENT* event) {
  // Cast context back to QuicServer instance.
  // Why: Static method cannot access instance members, so we use the context
  //      pointer that was passed during ListenerOpen.
  QuicServer* server = static_cast<QuicServer*>(context);
  if (server == nullptr) {
    return QUIC_STATUS_INVALID_PARAMETER;
  }

  return server->HandleListenerEvent(event);
}

QUIC_STATUS QuicServer::HandleListenerEvent(QUIC_LISTENER_EVENT* event) {
  switch (event->Type) {
    case QUIC_LISTENER_EVENT_NEW_CONNECTION: {
      // A new connection is attempting to connect.
      // Why: We accept the connection and assign our configuration to it.
      //      The connection handle is then passed to the user callback.
      HQUIC connection = event->NEW_CONNECTION.Connection;

      // Accept the connection with our configuration.
      QUIC_STATUS status = api_->ConnectionSetConfiguration(
          connection, config_->native());

      if (status != QUIC_STATUS_SUCCESS) {
        std::cerr << "[QuicServer] Failed to set connection configuration: "
                  << status << std::endl;
        return status;
      }

      // Invoke user callback if set.
      // Why: User callback allows application code to handle the new connection
      //      (e.g., create a QuicConnection wrapper, register stream callbacks, etc.).
      if (connection_callback_) {
        try {
          connection_callback_(connection, this);
        } catch (const std::exception& e) {
          std::cerr << "[QuicServer] Exception in connection callback: " << e.what()
                    << std::endl;
          return QUIC_STATUS_INTERNAL_ERROR;
        }
      } else {
        std::cout << "[QuicServer] New connection accepted (no callback set)"
                  << std::endl;
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
#else
int QuicServer::ListenerCallback(void* /*listener*/, void* /*context*/,
                                 void* /*event*/) {
  return 0;
}

int QuicServer::HandleListenerEvent(void* /*event*/) { return 0; }
#endif

void QuicServer::Cleanup() noexcept {
#ifdef QUICFLOW_HAS_MSQUIC
  if (listener_ != nullptr && api_ != nullptr) {
    api_->ListenerClose(listener_);
    listener_ = nullptr;
  }
#else
  if (listener_ != nullptr) {
    listener_ = nullptr;
  }
#endif

  is_listening_ = false;
}

}  // namespace network
}  // namespace quicflow

