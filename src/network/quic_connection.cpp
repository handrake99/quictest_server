//
// Created by 최진성 on 25. 12. 20..
//

#include "network/quic_connection.hpp"

#include <iostream>

#include "manager/connection_manager.hpp"
#include "network/quic_buffer_reader.hpp"
#include "network/quic_config_manager.hpp"
#include "network/quic_protocol.hpp"
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

DEFINE_ASYNC_FUNCTION(QuicConnection, SendChatMessage, const std::string& message) {
  if (stream_chat_ == nullptr) {
    std::cerr << "[QuicConnection] SendChatMessage called with nullptr" << std::endl;
    return;
  }

  // 1. JSON 객체 생성 (C# Dictionary보다 더 직관적)
  ChatProtocol jsonData;
  jsonData.Type = "Chat";
  jsonData.MessageId = message_id_;
  jsonData.UserID = "User1";
  jsonData.Message = message;
  jsonData.Timestamp = std::time(nullptr);

  message_id_++;

  json j = jsonData;

  // 2. 직렬화 (std::string으로 변환)
  std::string serializedStr = j.dump();
  SendJsonMessage(stream_chat_, serializedStr);
}

// 메시지를 Little Endian 헤더와 합쳐서 전송하는 함수
void QuicConnection::SendJsonMessage( const HQUIC hStream, const std::string& jsonMessage)
{
  uint32_t bodyLength = (uint32_t)jsonMessage.length();
  uint32_t totalLength = 4 + bodyLength;

  // 1. 단 하나의 버퍼만 할당 (Header + Body)
  // SendBufferContext를 힙에 생성하여 전송 완료 시점까지 살려둡니다.
  auto* SendCtx = new SendBufferContext(totalLength);
  uint8_t* BufferPtr = SendCtx->RawBuffer;

  // 2. [Little Endian] 헤더 작성 (4 Bytes)
  // CPU 아키텍처 상관없이 강제로 리틀 엔디안으로 박아넣음
  BufferPtr[0] = (uint8_t)(bodyLength & 0xFF);
  BufferPtr[1] = (uint8_t)((bodyLength >> 8) & 0xFF);
  BufferPtr[2] = (uint8_t)((bodyLength >> 16) & 0xFF);
  BufferPtr[3] = (uint8_t)((bodyLength >> 24) & 0xFF);

  // 3. 본문 복사 (Offset 4부터 시작)
  if (bodyLength > 0) {
    memcpy(BufferPtr + 4, jsonMessage.c_str(), bodyLength);
  }

  // 4. QUIC_BUFFER 구조체 세팅
  QUIC_BUFFER* QuicBuf = new QUIC_BUFFER();
//#ifdef _WIN32
  // Windows 방식
  QuicBuf->Length = totalLength;
  QuicBuf->Buffer = SendCtx->RawBuffer;
/*#else
  // Mac/Linux 방식 (iovec)
  // iovec 구조체로 캐스팅해서 넣어야 안전합니다.
  struct iovec* vec = (struct iovec*)&QuicBuf;
  vec->iov_base = SendCtx->RawBuffer; // 포인터가 먼저!
  vec->iov_len = totalLength;         // 길이가 나중!
#endif*/

  auto api = server_->config()->api(); // 실제 데이터가 들어있는 힙 메모리 주소

  printf("[DEBUG] Real Buffer Address (Heap): %p\n", SendCtx->RawBuffer);
  printf("[DEBUG] Intended Length: %u\n", totalLength);

  printf("Size Check: QUIC_BUFFER=%zu, iovec=%zu\n", sizeof(QUIC_BUFFER), sizeof(struct iovec));

  /*void** rawPtr = (void**)&RealVec;
  // QUIC_BUFFER 구조체 내부의 첫 8바이트와 두 번째 8바이트 확인
  printf("[DEBUG] Struct Slot 0 (First 8 bytes):  0x%llX\n", rawPtr[0]);
  printf("[DEBUG] Struct Slot 1 (Second 8 bytes): 0x%llX\n", rawPtr[1]);*/

  // 5. 전송 (비동기)
  // ClientSendContext 파라미터(마지막 인자)에 우리가 만든 SendCtx를 넘깁니다.
  // 이 포인터는 SEND_COMPLETE 이벤트에서 다시 돌려받아 delete 할 것입니다.
  QUIC_STATUS Status = api->StreamSend(
      hStream,
      QuicBuf,
      //(QUIC_BUFFER*)&RealVec,
      1,
      QUIC_SEND_FLAG_NONE,
      SendCtx // ★ 중요: 콜백으로 넘겨줄 문맥 포인터
  );

  if (QUIC_FAILED(Status)) {
    printf("[Error] StreamSend failed: 0x%x\n", Status);
    delete SendCtx; // 전송 실패 시 즉시 해제
  }

  std::cout << "[QuicConnection] SendJsonMessage Success " << totalLength << std::endl;
}

QUIC_STATUS QUIC_API QuicConnection::ServerConnectionCallback(HQUIC connection, void* context, QUIC_CONNECTION_EVENT* event) {

  auto quicConnectionPtr = static_cast<QuicConnection*>(context);
  auto quicConnection = std::static_pointer_cast<QuicConnection>(quicConnectionPtr->shared_from_this());

  if (quicConnection == nullptr) {
    std::cerr << "[QuicServer] Connection is nullptr" << std::endl;
    return QUIC_STATUS_INTERNAL_ERROR;
  }

  //auto quicConnection = std::make_shared<QuicConnection>(connectionPtr->connection());
  auto api = QuicServer::GetInstance().api();

  std::cout << "[QuicConnection] ServerConnectionCallback Called (" << connection << "),(" << event->Type << ")"<< std::endl;

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
      quicConnection->OnChatStreamStartedAsync(event->PEER_STREAM_STARTED.Stream);
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

DEFINE_ASYNC_FUNCTION(QuicConnection, OnChatStreamStarted, HQUIC hStream){
  if (hStream == nullptr) {
    std::cerr << "[QuicConnection] Stream is nullptr" << std::endl;
    return;
  }
  stream_chat_ = hStream;
  auto api = server_->api();
  if (api == nullptr) {
    std::cerr << "[QuicConnection] Server API is nullptr" << std::endl;
    return ;
  }

  api->SetCallbackHandler(stream_chat_, (void*)ServerChatCallback, this);
  std::cout << "[QuicConnection] Set ServerChatCallback Handler" << std::endl;
}

DEFINE_ASYNC_FUNCTION(QuicConnection, OnChatStreamClosed){
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

DEFINE_ASYNC_FUNCTION(QuicConnection, OnChatStreamReceived, QUIC_STREAM_EVENT* event) {
  const QUIC_BUFFER* buffer = event->RECEIVE.Buffers;
  uint32_t bufferCount = event->RECEIVE.BufferCount;

  std::string outputString;

  if (QuicBufferReader::TryParseStringMessage(buffer, bufferCount, outputString) == false) {
    std::cerr << "[QuicStream] Receive Buffer Error" << bufferCount << "!" << std::endl;
    return;
  }

  manager::ConnectionManager::GetInstance().OnReceiveChatMessage(std::static_pointer_cast<QuicConnection>(shared_from_this()), outputString);
}

// static callback
QUIC_STATUS QuicConnection::ServerChatCallback(HQUIC hStream, void* context, QUIC_STREAM_EVENT* event) {
  auto quicConnectionPtr = static_cast<QuicConnection*>(context);
  auto quicConnection = std::static_pointer_cast<QuicConnection>(quicConnectionPtr->shared_from_this());

  std::cout << "[QuicConnection] ServerMessageCallback Event Type! " << event->Type << std::endl;

  switch (event->Type) {
    case QUIC_STREAM_EVENT_RECEIVE:
      std::cout << "[QuicStream] Receive Event Type!" << std::endl;
      quicConnection->OnChatStreamReceivedAsync(event);
      break;
    case QUIC_STREAM_EVENT_SEND_COMPLETE:
      // ★ 핵심: 전송이 완료되었으므로 힙에 할당했던 JSON 문자열 해제
      if (event->SEND_COMPLETE.ClientContext) {
        auto payload = (SendBufferContext*)event->SEND_COMPLETE.ClientContext;

        printf("[DEBUG] Real Buffer Address (Heap): %p\n", payload->RawBuffer);
        printf("[DEBUG] Intended Length: %u\n", payload->TotalLength);

        delete payload; // 메모리 해제!
      }
      std::cout << "[QuicStream] STREAM Event SEND COMPLETE!" << std::endl;
      break;

    case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
      quicConnection->OnChatStreamClosedAsync();
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