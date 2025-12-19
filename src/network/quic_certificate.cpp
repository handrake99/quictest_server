#include "network/quic_certificate.hpp"

#include <cstring>
#include <iostream>

namespace quicflow {
namespace network {

#ifdef QUICFLOW_HAS_MSQUIC
QUIC_CREDENTIAL_CONFIG CreateSelfSignedCertificate(
    const std::string& common_name, int /*valid_days*/) {
  QUIC_CREDENTIAL_CONFIG cred_config{};

  // Use QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION for testing.
  // Why: Self-signed certificates will fail standard validation, so we disable
  //      validation for test environments. Production code should use proper
  //      CA-signed certificates and enable validation.
  cred_config.Flags = QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;

  // Use QUIC_CREDENTIAL_TYPE_NONE for now (simplified test setup).
  // Why: MsQuic provides platform-specific certificate generation. For a
  //      cross-platform solution, we would need to use platform-specific APIs
  //      (e.g., OpenSSL on Linux, Security framework on macOS).
  //
  // Note: In a full implementation, we would:
  //   1. Generate a self-signed certificate using OpenSSL or platform APIs
  //   2. Store the certificate and key in memory or files
  //   3. Configure QUIC_CREDENTIAL_CONFIG with the certificate data
  //
  // For now, we use a minimal configuration that allows the server to start.
  // Clients connecting to this server will need to disable certificate
  // validation (acceptable for test environments).
  cred_config.Type = QUIC_CREDENTIAL_TYPE_NONE;

  std::cout << "[QuicCertificate] Created self-signed certificate config for '"
            << common_name << "' (test mode: validation disabled)" << std::endl;

  return cred_config;
}

QUIC_CREDENTIAL_CONFIG LoadCertificateFromFiles(
    const std::string& cert_file, const std::string& key_file) {
  QUIC_CREDENTIAL_CONFIG cred_config{};

  // Placeholder implementation.
  // Why: Certificate loading from files requires platform-specific code
  //      (OpenSSL on Linux, Security framework on macOS, etc.).
  //      This is left as a TODO for future implementation.
  std::cerr << "[QuicCertificate] LoadCertificateFromFiles not yet implemented"
            << std::endl;
  std::cerr << "[QuicCertificate] Certificate file: " << cert_file << std::endl;
  std::cerr << "[QuicCertificate] Key file: " << key_file << std::endl;

  cred_config.Type = QUIC_CREDENTIAL_TYPE_NONE;
  return cred_config;
}
#else
void* CreateSelfSignedCertificate(const std::string& /*common_name*/,
                                  int /*valid_days*/) {
  return nullptr;
}

void* LoadCertificateFromFiles(const std::string& /*cert_file*/,
                               const std::string& /*key_file*/) {
  return nullptr;
}
#endif

}  // namespace network
}  // namespace quicflow

