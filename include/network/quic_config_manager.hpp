#pragma once

// QuicFlow-CPP - MsQuic Configuration Manager
// Why: QUIC_CONFIGURATION과 QUIC_CREDENTIAL_CONFIG 같은 MsQuic 설정 핸들을
//      RAII 패턴으로 관리하여 메모리 누수와 리소스 해제 누락을 방지합니다.
//      이 클래스는 QuicListener가 사용할 설정을 중앙에서 관리하며,
//      ALPN, 인증서, 커넥션 파라미터 등을 캡슐화합니다.

#include <cstdint>
#include <string>
#include <vector>

// Forward declaration to avoid circular dependency.
// Why: QuicConfigManager only needs a reference to QuicApi, not its
//      full definition. Forward declaration reduces compile-time dependencies.
namespace quicflow {
namespace network {
class QuicApi;
}  // namespace network
}  // namespace quicflow

#ifdef QUICFLOW_HAS_MSQUIC
extern "C" {
#include <msquic.h>
}
#endif

namespace quicflow {
namespace network {

// RAII wrapper for MsQuic configuration handles.
// This class manages the lifecycle of QUIC_CONFIGURATION and related
// credential structures, ensuring proper cleanup even in error paths.
//
// Usage example:
//   QuicConfigManager config(api, {"h3", "h3-29"});
//   if (!config.is_valid()) {
//     // Handle error
//   }
//   // Use config.native() to create listeners/connections
class QuicConfigManager {
 public:
  // Constructs a configuration manager with the given ALPN protocols.
  // Why: ALPN (Application-Layer Protocol Negotiation) is required for
  //      HTTP/3 and WebTransport. We accept a vector to support multiple
  //      protocol versions for backward compatibility.
  //
  // Args:
  //   api: Reference to initialized QuicApi instance. Must outlive this object.
  //   alpn_protocols: List of ALPN protocol strings (e.g., "h3", "h3-29").
  //                  Must not be empty.
  //
  // Postconditions:
  //   - If construction succeeds, is_valid() returns true.
  //   - If construction fails, is_valid() returns false and error details
  //     can be retrieved via error_message().
  explicit QuicConfigManager();

  // Non-copyable: configuration handles are unique resources.
  QuicConfigManager(const QuicConfigManager&) = delete;
  QuicConfigManager& operator=(const QuicConfigManager&) = delete;

  // Movable: allows transferring ownership of configuration handles.
  QuicConfigManager(QuicConfigManager&& other) noexcept;
  QuicConfigManager& operator=(QuicConfigManager&& other) noexcept;

  ~QuicConfigManager();

  // Returns true if the configuration was successfully initialized.
  // Why: MsQuic configuration creation can fail due to invalid parameters
  //      or resource constraints. Callers must check this before using
  //      native() to avoid undefined behavior.
  bool is_valid() const noexcept { return is_valid_; }

  // Returns the native QUIC_CONFIGURATION handle.
  // Why: QuicListener and other components need direct access to the
  //      configuration handle for MsQuic API calls. We return a pointer
  //      rather than a reference to match MsQuic's C API conventions.
  //
  // Preconditions:
  //   - is_valid() must return true.
  //
  // Returns:
  //   - Non-null pointer to QUIC_CONFIGURATION if valid.
  //   - nullptr if configuration is invalid (should not happen if
  //     is_valid() check is performed).
  HQUIC configuration() const noexcept;
  // Returns the registration handle used by this configuration.
  // Why: ListenerOpen requires a registration handle, not a configuration handle.
  HQUIC registration() const noexcept;
  const QUIC_API_TABLE* api() const noexcept;
  // Returns the ALPN buffers used by this configuration.
  // Why: ListenerStart requires the same ALPN buffers as ConfigurationOpen.
  const std::vector<QUIC_BUFFER>& alpn_buffers() const noexcept;

  // Returns a human-readable error message if initialization failed.
  // Why: Diagnostic information helps developers understand why
  //      configuration creation failed (e.g., invalid ALPN, missing
  //      certificate, etc.).
  const std::string& error_message() const noexcept { return error_message_; }

  bool InitializeConfig();

  // Sets the credential configuration for TLS/QUIC handshake.
  // Why: QUIC requires TLS 1.3, which needs certificate configuration.
  //      This method allows setting credentials after construction,
  //      supporting both self-signed and CA-signed certificates.
  //
  // Args:
  //   credential_config: QUIC_CREDENTIAL_CONFIG structure with certificate
  //                      and key information. The structure is copied, but
  //                      pointers within it (e.g., certificate data) must
  //                      remain valid until this object is destroyed.
  //
  // Returns:
  //   - true if credential was set successfully.
  //   - false if configuration is invalid or credential setting failed.
  bool set_credential(const QUIC_CREDENTIAL_CONFIG& credential_config);

 private:
  // Helper to initialize ALPN buffers from string vector.
  // Why: MsQuic requires QUIC_BUFFER structures for ALPN, but we want
  //      to accept std::string for convenience. This conversion is
  //      encapsulated here.
  void InitializeAlpnBuffers(const std::vector<std::string>& alpn_protocols);

  // Helper to clean up allocated resources.
  // Why: Both destructor and move assignment need cleanup logic.
  //      DRY principle: extract to a single method.
  void Cleanup() noexcept;

  const QUIC_API_TABLE* api_;
  HQUIC handle_registration_;  // QUIC_REGISTRATION is an alias for HQUIC
  HQUIC handle_config_;  // QUIC_CONFIGURATION is an alias for HQUIC
  std::vector<QUIC_BUFFER> alpn_buffers_;
  std::vector<std::vector<uint8_t>> alpn_storage_;  // Owns ALPN string data

  bool is_valid_;
  std::string error_message_;
};

}  // namespace network
}  // namespace quicflow

