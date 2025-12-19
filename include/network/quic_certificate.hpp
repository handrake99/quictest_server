#pragma once

// QuicFlow-CPP - QUIC Certificate Utilities
// Why: 테스트 환경을 위한 self-signed 인증서 생성 및 로드 유틸리티를 제공합니다.
//      프로덕션 환경에서는 CA-signed 인증서를 사용하지만, 개발/테스트 단계에서는
//      self-signed 인증서로 충분합니다.

#include <string>
#include <vector>

#ifdef QUICFLOW_HAS_MSQUIC
extern "C" {
#include <msquic.h>
}
#endif

namespace quicflow {
namespace network {

// Creates a self-signed certificate for testing purposes.
// Why: QUIC/TLS requires a certificate for the handshake. In test environments,
//      generating a self-signed certificate programmatically simplifies setup.
//
// Args:
//   common_name: Common name (CN) for the certificate (e.g., "localhost").
//   valid_days: Number of days the certificate should be valid (default: 365).
//
// Returns:
//   QUIC_CREDENTIAL_CONFIG structure configured for self-signed certificate.
//   The caller is responsible for ensuring the certificate data remains valid
//   for the lifetime of any QUIC configuration using it.
//
// Note: This is a simplified implementation for testing. Production code should
//       load certificates from files or certificate stores.
#ifdef QUICFLOW_HAS_MSQUIC
QUIC_CREDENTIAL_CONFIG CreateSelfSignedCertificate(
    const std::string& common_name = "localhost",
    int valid_days = 365);
#else
void* CreateSelfSignedCertificate(const std::string& /*common_name*/ = "localhost",
                                  int /*valid_days*/ = 365);
#endif

// Loads a certificate from PEM files.
// Why: 프로덕션 환경에서는 파일 시스템에 저장된 인증서를 로드해야 합니다.
//      이 함수는 PEM 형식의 인증서와 키 파일을 읽어서 QUIC_CREDENTIAL_CONFIG로 변환합니다.
//
// Args:
//   cert_file: Path to certificate file (PEM format).
//   key_file: Path to private key file (PEM format).
//
// Returns:
//   QUIC_CREDENTIAL_CONFIG structure, or nullptr if loading failed.
//
// Note: This is a placeholder for future implementation. MsQuic on different
//       platforms may require different certificate loading mechanisms.
#ifdef QUICFLOW_HAS_MSQUIC
QUIC_CREDENTIAL_CONFIG LoadCertificateFromFiles(
    const std::string& cert_file,
    const std::string& key_file);
#else
void* LoadCertificateFromFiles(const std::string& /*cert_file*/,
                                const std::string& /*key_file*/);
#endif

}  // namespace network
}  // namespace quicflow

