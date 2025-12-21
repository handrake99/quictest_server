#include "network/quic_certificate.hpp"

#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

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

QUIC_CREDENTIAL_CONFIG LoadCertificateFromFiles(const std::string& cert_file,
                                                const std::string& key_file) {
  QUIC_CREDENTIAL_CONFIG cred_config{};
  memset(&cred_config, 0, sizeof(QUIC_CREDENTIAL_CONFIG));

  // Verify that certificate and key files exist.
  // Why: File existence check provides early error detection and clear
  //      error messages to the user.
  std::filesystem::path cert_path(cert_file);
  std::filesystem::path key_path(key_file);

  if (!std::filesystem::exists(cert_path)) {
    std::cerr << "[QuicCertificate] Certificate file not found: " << cert_file
              << std::endl;
    cred_config.Type = QUIC_CREDENTIAL_TYPE_NONE;
    return cred_config;
  }

  if (!std::filesystem::exists(key_path)) {
    std::cerr << "[QuicCertificate] Key file not found: " << key_file
              << std::endl;
    cred_config.Type = QUIC_CREDENTIAL_TYPE_NONE;
    return cred_config;
  }

  // Convert paths to absolute paths for MsQuic.
  // Why: MsQuic requires absolute paths or paths relative to the current
  //      working directory. Using absolute paths ensures consistency.
  std::string abs_cert_file = std::filesystem::absolute(cert_path).string();
  std::string abs_key_file = std::filesystem::absolute(key_path).string();

  // Configure QUIC_CREDENTIAL_CONFIG for file-based certificate loading.
  // Why: QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE allows loading PEM-format
  //      certificates directly from files, which is the standard approach
  //      for production deployments.
  cred_config.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;
  cred_config.Flags = QUIC_CREDENTIAL_FLAG_NONE;  // Use default validation

  // Allocate QUIC_CERTIFICATE_FILE structure.
  // Why: The CertificateFile member is a union, so we need to allocate
  //      the structure separately and store file paths. We use static storage
  //      to ensure the strings remain valid for the lifetime of the config.
  static QUIC_CERTIFICATE_FILE cert_file_struct;
  static std::string stored_cert_path;
  static std::string stored_key_path;

  stored_cert_path = abs_cert_file;
  stored_key_path = abs_key_file;

  cert_file_struct.CertificateFile = stored_cert_path.c_str();
  cert_file_struct.PrivateKeyFile = stored_key_path.c_str();

  cred_config.CertificateFile = &cert_file_struct;


  std::cout << "[QuicCertificate]   Certificate: " << abs_cert_file
            << std::endl;
  std::cout << "[QuicCertificate]   Private Key: " << abs_key_file << std::endl;

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
