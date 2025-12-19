#include "network/quic_config_manager.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

#include "network/quic_api.hpp"
#include "network/quic_certificate.hpp"

namespace quicflow {
namespace network {

QuicConfigManager::QuicConfigManager()
    : api_(nullptr),
      handle_config_(nullptr),
      is_valid_(false)
{
}

QuicConfigManager::QuicConfigManager(QuicConfigManager&& other) noexcept
#ifdef QUICFLOW_HAS_MSQUIC
    : api_(other.api_),
      handle_config_(other.handle_config_),
      alpn_buffers_(std::move(other.alpn_buffers_)),
      alpn_storage_(std::move(other.alpn_storage_)),
      is_valid_(other.is_valid_),
      error_message_(std::move(other.error_message_))
#else
    : api_(other.api_),
      handle_config_(other.handle_config_),
      is_valid_(other.is_valid_),
      error_message_(std::move(other.error_message_))
#endif
{
  // Reset source object to prevent double cleanup.
  other.api_ = nullptr;
  other.handle_config_ = nullptr;
  other.is_valid_ = false;
  other.error_message_.clear();
}

QuicConfigManager& QuicConfigManager::operator=(
    QuicConfigManager&& other) noexcept {
  if (this != &other) {
    Cleanup();

#ifdef QUICFLOW_HAS_MSQUIC
    api_ = other.api_;
    handle_config_ = other.handle_config_;
    alpn_buffers_ = std::move(other.alpn_buffers_);
    alpn_storage_ = std::move(other.alpn_storage_);
#else
    api_ = other.api_;
    handle_config_ = other.handle_config_;
#endif

    is_valid_ = other.is_valid_;
    error_message_ = std::move(other.error_message_);

    // Reset source object.
    other.api_ = nullptr;
    other.handle_config_ = nullptr;
    other.is_valid_ = false;
    other.error_message_.clear();
  }
  return *this;
}

QuicConfigManager::~QuicConfigManager() { Cleanup(); }

#ifdef QUICFLOW_HAS_MSQUIC
HQUIC QuicConfigManager::configuration() const noexcept {
  return is_valid_ ? handle_config_ : nullptr;
}

HQUIC QuicConfigManager::registration() const noexcept {
  return is_valid_ ? handle_registration_ : nullptr;
}

const QUIC_API_TABLE* QuicConfigManager::api() const noexcept {
  return is_valid_ ? api_ : nullptr;
}

const std::vector<QUIC_BUFFER>& QuicConfigManager::alpn_buffers() const noexcept {
  return alpn_buffers_;
}
#else
const void* QuicConfigManager::native() const noexcept {
  return nullptr;
}

const void* QuicConfigManager::registration() const noexcept {
  return nullptr;
}

const std::vector<void*>& QuicConfigManager::alpn_buffers() const noexcept {
  static std::vector<void*> empty;
  return empty;
}
#endif


#ifdef QUICFLOW_HAS_MSQUIC
bool QuicConfigManager::InitializeConfig() {
  // Initialize MsQuic API.
  QuicApi api{};
  if (!api.is_available()) {
    std::cerr << "[QuicFlow] MsQuic API not available at runtime. "
              << "The container may be missing libmsquic or QUICFLOW_HAS_MSQUIC "
              << "was not defined at compile time.\n";
    return false;
  }
 //std::shared_ptr<QuicConfigManager> config(new QuicConfigManager(api, {"h3", "h3-29", "webtransport"}));
  api_ = api.native();
  handle_config_ = nullptr;
  is_valid_ = false;
  std::vector<std::string> alpn_protocols = {"h3", "h3-29", "webtransport"};
  if (alpn_protocols.empty()) {
    error_message_ = "ALPN protocols list cannot be empty";
    return false;
  }

  // Convert std::string ALPN protocols to QUIC_BUFFER format.
  // Why: MsQuic's C API requires QUIC_BUFFER structures, but we accept
  //      std::string for type safety and convenience in C++ code.
  InitializeAlpnBuffers(alpn_protocols);

  QUIC_REGISTRATION_CONFIG RegConfig = { "quicflow App", QUIC_EXECUTION_PROFILE_LOW_LATENCY };

  // 1. Registration을 먼저 엽니다.
  QUIC_STATUS Status = api_->RegistrationOpen(&RegConfig, &handle_registration_);

  if (QUIC_FAILED(Status)) {
    // Registration 생성 실패 시 처리
    return false;
  }

  // Create QUIC configuration with default settings.
  // Why: We use QUIC_SETTINGS defaults initially. Advanced tuning
  //      (e.g., connection migration, idle timeout) can be added later
  //      via additional setter methods if needed.
  QUIC_SETTINGS settings = {};
  settings.IdleTimeoutMs = 60000;  // 60 seconds default
  settings.IsSet.IdleTimeoutMs = TRUE;
  settings.PeerBidiStreamCount = 100;  // Allow bidirectional streams
  settings.IsSet.PeerBidiStreamCount = TRUE;

  // Note: ConfigurationOpen requires a registration handle.
  // For now, we pass nullptr to use the default registration.
  // In a full implementation, we would create a QUIC_REGISTRATION
  // handle via MsQuic->RegistrationOpen() and manage it separately.
  QUIC_STATUS status = api_->ConfigurationOpen(
      handle_registration_,  // Use default registration (nullptr means use global)
      &alpn_buffers_[0],
      static_cast<uint32_t>(alpn_buffers_.size()),
      &settings,
      sizeof(settings),
      nullptr,  // Context (can be used for callbacks)
      &handle_config_);

  if (QUIC_FAILED(status)) {
    error_message_ = "Failed to create QUIC configuration: status " +
                     std::to_string(status);
    Cleanup();
    return false;
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
    return false;
  }

  if (!set_credential(cred_config)) {
    std::cerr << "[QuicFlow] Failed to set certificate: "
              << error_message() << std::endl;
    return false;
  }
#endif
  is_valid_ = true;

  return true;
#else
  error_message_ = "QUICFLOW_HAS_MSQUIC is not defined at compile time";
  return true;
#endif
}

#ifdef QUICFLOW_HAS_MSQUIC
bool QuicConfigManager::set_credential(const QUIC_CREDENTIAL_CONFIG& credential_config) {
  if (api_ == nullptr || handle_config_ == nullptr) {
    error_message_ = "Configuration is not valid";
    return false;
  }

  QUIC_STATUS status = api_->ConfigurationLoadCredential(handle_config_, &credential_config);

  if (QUIC_FAILED(status)) {
    error_message_ = "Failed to load credential: status " + std::to_string(status);
    return false;
  }
  std::cerr << "[DEBUG]" << R"({"config_handler":")" << handle_config_  << R"("})";

  return true;
}
#endif

void QuicConfigManager::InitializeAlpnBuffers(
    const std::vector<std::string>& alpn_protocols) {
#ifdef QUICFLOW_HAS_MSQUIC
  alpn_buffers_.clear();
  alpn_storage_.clear();
  alpn_buffers_.reserve(alpn_protocols.size());
  alpn_storage_.reserve(alpn_protocols.size());

  for (const auto& protocol : alpn_protocols) {
    // Store the string data in a vector to ensure lifetime.
    // Why: QUIC_BUFFER contains a pointer to the ALPN string, so we
    //      must keep the underlying data alive for the lifetime of
    //      the configuration.
    std::vector<uint8_t> storage(protocol.begin(), protocol.end());
    alpn_storage_.push_back(std::move(storage));

    QUIC_BUFFER buffer{};
    buffer.Length = static_cast<uint32_t>(alpn_storage_.back().size());
    buffer.Buffer = alpn_storage_.back().data();

    alpn_buffers_.push_back(buffer);
  }
#endif
}

void QuicConfigManager::Cleanup() noexcept {
#ifdef QUICFLOW_HAS_MSQUIC
  if (handle_config_ != nullptr && api_ != nullptr) {
    api_->ConfigurationClose(handle_config_);
    handle_config_ = nullptr;
  }
#else
  if (handle_config_ != nullptr) {
    handle_config_ = nullptr;
  }
#endif

  is_valid_ = false;
}

}  // namespace network
}  // namespace quicflow

