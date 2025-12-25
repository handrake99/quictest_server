//
// Created by 최진성 on 25. 12. 20..
//

#ifndef QUICFLOWCPP_QUIC_CONNECTION_HPP
#define QUICFLOWCPP_QUIC_CONNECTION_HPP

#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
extern "C" {
#include <msquic.h>
}
using json = nlohmann::json;

namespace quicflow {
namespace network {
class QuicServer;
class QuicApi;
class QuicConfigManager;

// 전송이 끝날 때까지 메모리를 살려두기 위한 컨텍스트
struct SendContext {
  std::string data; // 실제 JSON 문자열

  SendContext(std::string d) : data(std::move(d)) {}
};

class QuicConnection : public std::enable_shared_from_this<QuicConnection> {
public:
  QuicConnection(HQUIC connection);

  //QUIC_STATUS InitConnection(const QUIC_API_TABLE* api,  std::shared_ptr<QuicConfigManager> config);
  QUIC_STATUS InitConnection(QuicServer* server);
  void CloseConnection();

  void SendJsonMessage(const std::string& content);

  static QUIC_STATUS ServerConnectionCallback(HQUIC connection, void* context, QUIC_CONNECTION_EVENT* Event);
  static QUIC_STATUS ServerStreamCallback(HQUIC connection, void* context, QUIC_STREAM_EVENT* Event);

  HQUIC connection() { return connection_; }

private:
  QuicServer* server_;
  HQUIC connection_;

  volatile int message_id_ = 0;
};
};
}
#endif  // QUICFLOWCPP_QUIC_CONNECTION_HPP
