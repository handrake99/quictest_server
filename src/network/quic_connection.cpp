//
// Created by 최진성 on 25. 12. 20..
//

#include "network/quic_connection.hpp"

#include <iostream>

#include "network/quic_config_manager.hpp"
#include "network/quic_server.hpp"

namespace quicflow {
namespace network {

QuicConnection::QuicConnection(HQUIC connection) {
  connection_ = connection;
}
QUIC_STATUS QuicConnection::InitConnection(const QUIC_API_TABLE* api,  std::shared_ptr<QuicConfigManager> config) {

  if ( api == nullptr
    || config == nullptr
    || connection_ == nullptr) {
    return QUIC_STATUS_INTERNAL_ERROR;
  }

  api->SetCallbackHandler(connection_, (void*)OnPacketCallback, this);

  // 3. 설정(Configuration) 적용
  // 주의: configuration_ 핸들이 유효해야 함
  QUIC_STATUS status = api->ConnectionSetConfiguration(connection_, config->configuration());
  if (QUIC_FAILED(status)) {
    std::cerr << "[QuicServer] Failed to set connection configuration: "
              << status << " (0x" << std::hex << status << std::dec << ")" << std::endl;

    return status;
  }

  return QUIC_STATUS_SUCCESS;
}

void QuicConnection::CloseConnection() {
  server_ = nullptr;

  if (connection_ != nullptr) {
  }
}

void QuicConnection::OnPacketCallback(HQUIC connection, void* context) {
    std::clog << "[DEBUG][F] OnPacketCallback Called (" << connection << ")" << std::endl;
}

}  // namespace network
}