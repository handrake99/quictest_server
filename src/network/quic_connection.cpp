//
// Created by 최진성 on 25. 12. 20..
//

#include "network/quic_connection.hpp"

#include <iostream>

#include "manager/connection_manager.hpp"
#include "network/quic_config_manager.hpp"
#include "network/quic_server.hpp"

namespace quicflow {
namespace network {

QuicConnection::QuicConnection(HQUIC connection) {
  connection_ = connection;
}
//QUIC_STATUS QuicConnection::InitConnection(const QUIC_API_TABLE* api,  std::shared_ptr<QuicConfigManager> config) {
QUIC_STATUS QuicConnection::InitConnection(QuicServer* server) {
  if (server == nullptr) {
    return QUIC_STATUS_INTERNAL_ERROR;
  }
  server_ = server;
  auto api = server_->api();
  auto config = server_->config();

  if ( api == nullptr
    || config == nullptr
    || connection_ == nullptr) {
    return QUIC_STATUS_INTERNAL_ERROR;
  }

  api->SetCallbackHandler(connection_, (void*)ServerConnectionCallback, this);

  std::cout << "[QuicConnection] Regist Handler and Context "  << &ServerConnectionCallback << ", " << this << std::endl;

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

  if (connection_ != nullptr) {
    auto api = server_->api();
    api->ConnectionClose(connection_);
  }

  server_ = nullptr;
}
void QuicConnection::SendChatMessage(const std::string& content) {
  if (stream_chat_ == nullptr) {
    std::cerr << "[QuicConnection] SendChatMessage called with nullptr" << std::endl;
    return;
  }

  SendJsonMessage(stream_chat_, content);
}
void QuicConnection::SendJsonMessage(HQUIC hStream, const std::string& content) {
  if (hStream == nullptr) {
    std::cerr << "[QuicConnection] SendJsonMessage called with nullptr" << std::endl;
    return;
  }
  // 1. JSON 객체 생성 (C# Dictionary보다 더 직관적)
  json j;
  j["msg_id"] = message_id_;
  j["content"] = content;
  j["timestamp"] = std::time(nullptr);
  j["extra_info"] = { {"hp", 100}, {"mp", 50} }; // 중첩 객체도 가능

  message_id_++;

  // 2. 직렬화 (std::string으로 변환)
  std::string serializedStr = j.dump();

  // 3. [중요] 힙에 할당 (비동기 전송 생명주기 보장용)
  // 이 객체는 위의 Callback에서 delete 됩니다.
  SendContext* payload = new SendContext(serializedStr);

  // 4. QUIC 버퍼 세팅
  QUIC_BUFFER quicBuf;
  quicBuf.Length = (uint32_t)payload->data.length();
  quicBuf.Buffer = (uint8_t*)payload->data.c_str();

  // 5. 스트림 열기 & 보내기
  HQUIC stream = nullptr;
  auto api = server_->config()->api();
  api->StreamSend(
      stream,
      &quicBuf,
      1,
      QUIC_SEND_FLAG_NONE, // 메시지 보내고 스트림 닫음 (단발성)
      payload // <--- 이 포인터가 나중에 Callback으로 돌아옵니다.
  );
}

QUIC_STATUS QUIC_API QuicConnection::ServerConnectionCallback(HQUIC connection, void* context, QUIC_CONNECTION_EVENT* event) {

  std::cout << "[QuicConnection] ServerConnectionCallback Context "  << context << std::endl;
  auto quicConnectionPtr = static_cast<QuicConnection*>(context);
  auto quicConnection = quicConnectionPtr->shared_from_this();

  if (quicConnection == nullptr) {
    std::cerr << "[QuicServer] Connection is nullptr" << std::endl;
    return QUIC_STATUS_INTERNAL_ERROR;
  }

  //auto quicConnection = std::make_shared<QuicConnection>(connectionPtr->connection());
  auto api = QuicServer::GetInstance().api();

  std::cout << "[Conn] ServerConnectionCallback Called (" << connection << "),(" << event->Type << ")"<< std::endl;

  switch (event->Type) {
    // [연결 성공] 핸드셰이크 완료
    case QUIC_CONNECTION_EVENT_CONNECTED:{
      std::cout << "[Conn] Client Connected!" << std::endl;
      //quicConnection->SendJsonMessage("Welcome to Server");
      break;
    }
    case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT: {
      std::cout << "[Conn] Client Connection SHUTDOWN INITIATED BY TRANSPORT!" << std::endl;
      QUIC_STATUS innerStatus = event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status;
      QUIC_STATUS innerErrorCode = event->SHUTDOWN_INITIATED_BY_TRANSPORT.ErrorCode;

      // Status 코드를 찍어봐야 합니다.
      // 리눅스/맥에서는 0x..., 윈도우에서는 음수/양수 등으로 나옴
      std::cout << "[Conn] Transport Shutdown! Status: " << innerStatus << "," << innerErrorCode << std::endl;
      break;
    }

    // [연결 종료]
    case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE: {
      std::cout << "[Conn] Closed." << std::endl;
      manager::ConnectionManager::GetInstance().OnCloseConnection(quicConnection);

      break;
    }

    // [스트림 수신] 클라이언트가 먼저 스트림을 열어서 데이터를 보냈을 때
    case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED: {
      // 이 스트림을 처리할 콜백 지정
      quicConnection->OnChatStreamStarted(event->PEER_STREAM_STARTED.Stream);
      std::cout << "[Conn] Connection Event Pear Stream Started!" << std::endl;
      break;
    }

    default: {
      std::cout << "[Conn] Connection Event Type!" << event->Type <<std::endl;
      break;
    }

  }
  return QUIC_STATUS_SUCCESS;
}
void QuicConnection::OnChatStreamStarted(HQUIC hStream) {
  if (hStream == nullptr) {
    std::cerr << "[QuicConnection] Stream is nullptr" << std::endl;
    return;
  }
  stream_chat_ = hStream;
  auto api = server_->api();
  if (api == nullptr) {
    std::cerr << "[QuicServer] Server API is nullptr" << std::endl;
    return ;
  }

  api->SetCallbackHandler(stream_chat_, (void*)ServerChatCallback, this);
}

void QuicConnection::OnChatStreamClosed() {
  if (stream_chat_== nullptr) {
    std::cerr << "[QuicConnection] Chat Stream is nullptr" << std::endl;
    return;
  }

  auto api = server_->api();
  if (api == nullptr) {
    std::cerr << "[QuicServer] Server API is nullptr" << std::endl;
    return ;
  }

  api->StreamClose(stream_chat_);
  stream_chat_ = nullptr;
}
void QuicConnection::OnChatStreamReceived(QUIC_STREAM_EVENT* event) {

  const QUIC_BUFFER* buffers = event->RECEIVE.Buffers;
  uint32_t bufferCount = event->RECEIVE.BufferCount;

  for (uint32_t i=0; i< bufferCount; i++) {
    std::cout << "[QuicStream] Receive Buffer " << i << "!" << std::endl;
  }
}


// static callback
QUIC_STATUS QuicConnection::ServerChatCallback(HQUIC hStream, void* context, QUIC_STREAM_EVENT* event) {
  auto quicConnectionPtr = static_cast<QuicConnection*>(context);
  auto quicConnection = quicConnectionPtr->shared_from_this();

  switch (event->Type) {
    case QUIC_STREAM_EVENT_RECEIVE:
      std::cout << "[QuicStream] Receive Event Type!" << std::endl;
      quicConnection->OnChatStreamReceived(event);
      break;
    case QUIC_STREAM_EVENT_SEND_COMPLETE:
      // ★ 핵심: 전송이 완료되었으므로 힙에 할당했던 JSON 문자열 해제
      if (event->SEND_COMPLETE.ClientContext) {
        auto payload = (SendContext*)event->SEND_COMPLETE.ClientContext;
        delete payload; // 메모리 해제!
        // printf("[Stream] JSON Sent & Memory Freed\n");
      }
      break;

    case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
      quicConnection->OnChatStreamClosed();
      break;
    default:
      std::cout << "[QuicStream] Stream Event Type!" << event->Type << std::endl;
      break;

      // ... 기타 에러 처리 ...
  }
  return QUIC_STATUS_SUCCESS;
}

}  // namespace network
}