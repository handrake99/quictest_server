#include "network/quic_config_manager.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>

#include "network/quic_api.hpp"

namespace quicflow {
namespace network {

QuicConfigManager::QuicConfigManager(const QuicApi& api,
                                     const std::vector<std::string>& alpn_protocols)
#ifdef QUICFLOW_HAS_MSQUIC
    : api_(api.native()),
      configuration_(nullptr),
      is_valid_(false)
#else
    : api_(nullptr),
      configuration_(nullptr),
      is_valid_(false)
#endif
{
  if (alpn_protocols.empty()) {
    error_message_ = "ALPN protocols list cannot be empty";
    return;
  }

#ifdef QUICFLOW_HAS_MSQUIC
  if (api_ == nullptr) {
    error_message_ = "QuicApi is not available";
    return;
  }

  // Convert std::string ALPN protocols to QUIC_BUFFER format.
  // Why: MsQuic's C API requires QUIC_BUFFER structures, but we accept
  //      std::string for type safety and convenience in C++ code.
  InitializeAlpnBuffers(alpn_protocols);

  // Create QUIC configuration with default settings.
  // Why: We use QUIC_SETTINGS defaults initially. Advanced tuning
  //      (e.g., connection migration, idle timeout) can be added later
  //      via additional setter methods if needed.
  QUIC_SETTINGS settings{};
  settings.IdleTimeoutMs = 60000;  // 60 seconds default
  settings.IsSet.IdleTimeoutMs = TRUE;
  settings.PeerBidiStreamCount = 100;  // Allow bidirectional streams
  settings.IsSet.PeerBidiStreamCount = TRUE;

  // Note: ConfigurationOpen requires a registration handle.
  // For now, we pass nullptr to use the default registration.
  // In a full implementation, we would create a QUIC_REGISTRATION
  // handle via MsQuic->RegistrationOpen() and manage it separately.
  QUIC_STATUS status = api_->ConfigurationOpen(
      nullptr,  // Use default registration (nullptr means use global)
      &alpn_buffers_[0],
      static_cast<uint32_t>(alpn_buffers_.size()),
      &settings,
      sizeof(settings),
      nullptr,  // Context (can be used for callbacks)
      &configuration_);

  if (status != QUIC_STATUS_SUCCESS) {
    error_message_ = "Failed to create QUIC configuration: status " +
                     std::to_string(status);
    Cleanup();
    return;
  }

  is_valid_ = true;
#else
  error_message_ = "QUICFLOW_HAS_MSQUIC is not defined at compile time";
#endif
}

QuicConfigManager::QuicConfigManager(QuicConfigManager&& other) noexcept
#ifdef QUICFLOW_HAS_MSQUIC
    : api_(other.api_),
      configuration_(other.configuration_),
      alpn_buffers_(std::move(other.alpn_buffers_)),
      alpn_storage_(std::move(other.alpn_storage_)),
      is_valid_(other.is_valid_),
      error_message_(std::move(other.error_message_))
#else
    : api_(other.api_),
      configuration_(other.configuration_),
      is_valid_(other.is_valid_),
      error_message_(std::move(other.error_message_))
#endif
{
  // Reset source object to prevent double cleanup.
  other.api_ = nullptr;
  other.configuration_ = nullptr;
  other.is_valid_ = false;
  other.error_message_.clear();
}

QuicConfigManager& QuicConfigManager::operator=(
    QuicConfigManager&& other) noexcept {
  if (this != &other) {
    Cleanup();

#ifdef QUICFLOW_HAS_MSQUIC
    api_ = other.api_;
    configuration_ = other.configuration_;
    alpn_buffers_ = std::move(other.alpn_buffers_);
    alpn_storage_ = std::move(other.alpn_storage_);
#else
    api_ = other.api_;
    configuration_ = other.configuration_;
#endif

    is_valid_ = other.is_valid_;
    error_message_ = std::move(other.error_message_);

    // Reset source object.
    other.api_ = nullptr;
    other.configuration_ = nullptr;
    other.is_valid_ = false;
    other.error_message_.clear();
  }
  return *this;
}

QuicConfigManager::~QuicConfigManager() { Cleanup(); }

#ifdef QUICFLOW_HAS_MSQUIC
HQUIC QuicConfigManager::native() const noexcept {
  return is_valid_ ? configuration_ : nullptr;
}
#else
const void* QuicConfigManager::native() const noexcept {
  return nullptr;
}
#endif

#ifdef QUICFLOW_HAS_MSQUIC
bool QuicConfigManager::set_credential(
    const QUIC_CREDENTIAL_CONFIG& credential_config) {
  if (!is_valid_ || api_ == nullptr || configuration_ == nullptr) {
    error_message_ = "Configuration is not valid";
    return false;
  }

  QUIC_STATUS status =
      api_->ConfigurationLoadCredential(configuration_, &credential_config);

  if (status != QUIC_STATUS_SUCCESS) {
    error_message_ = "Failed to load credential: status " +
                     std::to_string(status);
    return false;
  }

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
  if (configuration_ != nullptr && api_ != nullptr) {
    api_->ConfigurationClose(configuration_);
    configuration_ = nullptr;
  }
#else
  if (configuration_ != nullptr) {
    configuration_ = nullptr;
  }
#endif

  is_valid_ = false;
}

}  // namespace network
}  // namespace quicflow

