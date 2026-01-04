//
// Created by 최진성 on 25. 12. 20..
//

#ifndef QUICFLOWCPP_QUIC_CONNECTION_HPP
#define QUICFLOWCPP_QUIC_CONNECTION_HPP

#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

#include "core/serialized_task.hpp"
extern "C" {
#include <msquic.h>
}
using json = nlohmann::json;

namespace quicflow {

namespace network {

using namespace quicflow::core;

class QuicServer;
class QuicApi;
class QuicConfigManager;
//class SerializedObject;
#include "core/serialized_object.hpp"

// [핵심] 전송이 끝날 때까지 메모리를 유지하기 위한 구조체
struct SendBufferContext {
  uint8_t* RawBuffer;
  uint32_t TotalLength;

  SendBufferContext(uint32_t size) {
    RawBuffer = new uint8_t[size];
    TotalLength = size;
  }

  ~SendBufferContext() {
    delete[] RawBuffer;
  }
};

// 1 유저 1개의 Connection 객체
//class QuicConnection : public std::enable_shared_from_this<QuicConnection> {
class QuicConnection : public SerializedObject {
public:
  QuicConnection(HQUIC connection);

  //QUIC_STATUS InitConnection(const QUIC_API_TABLE* api,  std::shared_ptr<QuicConfigManager> config);
  QUIC_STATUS InitConnection(QuicServer* server);
  void CloseConnection();

  void OnChatStreamStartedAsync(HQUIC hStream);

  void OnChatStreamStarted(HQUIC hStream);
  void OnChatStreamReceived(QUIC_STREAM_EVENT* event);
  void OnChatStreamClosed();

  void SendChatMessage(const std::string& content);

  static QUIC_STATUS ServerConnectionCallback(HQUIC connection, void* context, QUIC_CONNECTION_EVENT* Event);
  static QUIC_STATUS ServerChatCallback(HQUIC connection, void* context, QUIC_STREAM_EVENT* Event);

  HQUIC connection() { return connection_; }


private:
  void SendJsonMessage(HQUIC hStream, const std::string& message);

  QuicServer* server_;
  HQUIC connection_;
  HQUIC stream_chat_;

  volatile uint32_t message_id_ = 0;
};
};
}
#endif  // QUICFLOWCPP_QUIC_CONNECTION_HPP
