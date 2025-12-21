//
// Created by 최진성 on 25. 12. 20..
//

#ifndef QUICFLOWCPP_QUIC_CONNECTION_HPP
#define QUICFLOWCPP_QUIC_CONNECTION_HPP

#ifdef QUICFLOW_HAS_MSQUIC
#include <functional>
#include <memory>
extern "C" {
#include <msquic.h>
}


#endif
namespace quicflow {
namespace network {
class QuicServer;
class QuicApi;
class QuicConfigManager;

class QuicConnection {
public:
  QuicConnection(HQUIC connection);

  QUIC_STATUS InitConnection(const QUIC_API_TABLE* api,  std::shared_ptr<QuicConfigManager> config);
  void CloseConnection();

  static void OnPacketCallback(HQUIC connection, void* context);

  HQUIC connection() { return connection_; }

private:
  QuicServer* server_;
  HQUIC connection_;
};
};
}
#endif  // QUICFLOWCPP_QUIC_CONNECTION_HPP
