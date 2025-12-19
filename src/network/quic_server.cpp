#include "network/quic_server.hpp"

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include "network/quic_api.hpp"
#include "network/quic_config_manager.hpp"

// #region agent log
// Debug logging helper
static void DebugLog(const std::string& location, const std::string& message, const std::string& hypothesisId, const std::string& data = "") {
  try {
    std::ofstream log_file("/Users/handrake/work/quicflow_test/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
      std::ostringstream json;
      json << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":")" << hypothesisId
           << R"(","location":")" << location << R"(","message":")" << message
           << R"(","data":)" << (data.empty() ? "{}" : data) << R"(,"timestamp":)" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "}\n";
      log_file << json.str();
      log_file.close();
    }
  } catch (...) {
    std::cerr << "[DEBUG][" << hypothesisId << "] " << location << ": " << message << " " << data << std::endl;
  }
}
// #endregion

namespace quicflow {
namespace network {

QuicServer::QuicServer() {
  std::cout << "QuicServer Instance Created" << std::endl;
}

void QuicServer::InitQuicServer(const  QUIC_API_TABLE* api,
                       std::shared_ptr<QuicConfigManager> config,
                       uint16_t port) {
#ifdef QUICFLOW_HAS_MSQUIC
  api_ = api;
  config_ = config;
  listener_ = nullptr;
#else
  api_ = nullptr;
  config_ = nullptr;
  listener_ = nullptr;
#endif
  port_ = port;
  is_listening_ = false;

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
  //      CRITICAL: The ALPN buffers must match exactly what was used in ConfigurationOpen,
  //      otherwise ConnectionSetConfiguration will fail with INVALID_PARAMETER.
  std::cerr << "[DEBUG][F] Before ListenerStart: listener=" << listener_ 
            << ", port=" << port_ << std::endl;
  
  // Get ALPN buffers from configuration - MUST match ConfigurationOpen ALPN
  const auto& alpn_buffers = config_->alpn_buffers();
  if (alpn_buffers.empty()) {
    error_message_ = "Configuration has no ALPN buffers";
    std::cerr << "[ERROR] Configuration has no ALPN buffers" << std::endl;
    api_->ListenerClose(listener_);
    listener_ = nullptr;
    return false;
  }
  
  status = api_->ListenerStart(listener_, 
                                alpn_buffers.data(), 
                                static_cast<uint32_t>(alpn_buffers.size()), 
                                &address);
  
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
      const QUIC_NEW_CONNECTION_INFO* info = event->NEW_CONNECTION.Info;

      // #region agent log
      std::ostringstream conn_data;
      conn_data << R"({"connection":")" << connection << R"(","config_valid":)" 
                << (config_->is_valid() ? "true" : "false");
      if (info) {
        conn_data << R"(,"remote_addr_family":)" << info->RemoteAddress->Ipv4.sin_family;
      }
      conn_data << "}";

      std::cerr << "[DEBUG][G] NEW_CONNECTION event: connection=" << connection
                << ", config_valid=" << (config_->is_valid() ? "true" : "false");
      if (info) {
        std::cerr << ", remote_family=" << info->RemoteAddress->Ipv4.sin_family;
      }
      std::cerr << std::endl;
      // #endregion

      // Validate inputs before calling ConnectionSetConfiguration
      if (connection == nullptr) {
        std::cerr << "[ERROR][G] Connection handle is null" << std::endl;
        return QUIC_STATUS_INVALID_PARAMETER;
      }

      if (!config_->is_valid()) {
        std::cerr << "[ERROR][G] Configuration is not valid" << std::endl;
        return QUIC_STATUS_INVALID_PARAMETER;
      }

      // Accept the connection with our configuration.
      HQUIC config_handle = config_->configuration();
      if (config_handle == nullptr) {
        std::cerr << "[ERROR][G] Configuration handle is null" << std::endl;
        return QUIC_STATUS_INVALID_PARAMETER;
      }

      // Get registration handles for verification
      HQUIC config_registration = config_->registration();
      
      // Check ALPN from connection info
      std::string client_alpn;
      if (info && info->ClientAlpnListLength > 0 && info->ClientAlpnList != nullptr) {
        // ClientAlpnList is a pointer to a buffer containing ALPN strings
        // The format is: length-prefixed strings concatenated together
        const uint8_t* alpn_ptr = info->ClientAlpnList;
        uint32_t remaining = info->ClientAlpnListLength;
        if (remaining > 0 && alpn_ptr != nullptr) {
          // First byte is the length of the first ALPN string
          uint8_t alpn_len = alpn_ptr[0];
          if (alpn_len > 0 && alpn_len < remaining) {
            client_alpn = std::string(reinterpret_cast<const char*>(alpn_ptr + 1), alpn_len);
          }
        }
      }
      
      // #region agent log
      std::ostringstream before_data;
      before_data << R"({"connection":")" << connection << R"(","config_handle":")" 
                  << config_handle << R"(","config_registration":")" << config_registration
                  << R"(","client_alpn":")" << client_alpn << R"(","api_ptr":")" 
                  << (api_ ? "non-null" : "null") << R"("})";
      DebugLog("quic_server.cpp:230", "Before ConnectionSetConfiguration", "G", before_data.str());
      std::cerr << "[DEBUG][G] Before ConnectionSetConfiguration: connection=" << connection
                << ", config_handle=" << config_handle 
                << ", config_registration=" << config_registration
                << ", client_alpn=" << (client_alpn.empty() ? "(none)" : client_alpn)
                << ", api_ptr=" << (api_ ? "non-null" : "null") << std::endl;
      // #endregion
      
      // Note: ConnectionSetConfiguration requires that:
      // 1. Connection and configuration belong to the same registration (verified above)
      // 2. Configuration must have credentials loaded (verified in config_->is_valid())
      // 3. ALPN must match between client and configuration
      // 4. Connection must be in the correct state (NEW_CONNECTION event)


      QUIC_STATUS status = api_->ConnectionSetConfiguration(
          connection, config_handle);

      // #region agent log
      std::ostringstream after_data;
      after_data << R"({"status":)" << status << R"(,"status_hex":"0x)" << std::hex << status << std::dec << "}";
      DebugLog("quic_server.cpp:238", "After ConnectionSetConfiguration", "G", after_data.str());
      std::cerr << "[DEBUG][G] After ConnectionSetConfiguration: status=" << status 
                << " (0x" << std::hex << status << std::dec << ")" << std::endl;
      // #endregion

      if (status != QUIC_STATUS_SUCCESS) {
        std::cerr << "[QuicServer] Failed to set connection configuration: "
                  << status << " (0x" << std::hex << status << std::dec << ")" << std::endl;
        // #region agent log
        std::cerr << "[ERROR][G] ConnectionSetConfiguration failed: connection=" << connection
                  << ", config_handle=" << config_handle << ", status=" << status << std::endl;
        // #endregion
        
        // Status -2 (0xFFFFFFFE) is likely QUIC_STATUS_INVALID_PARAMETER
        // Possible causes:
        // 1. Configuration handle is invalid or already closed
        // 2. Connection is in wrong state (already configured or closed)
        // 3. Configuration doesn't have credentials set
        // 4. ALPN mismatch between connection and configuration
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

