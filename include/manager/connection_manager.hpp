//
// Created by 최진성 on 25. 12. 20..
//

#ifndef QUICFLOWCPP_CONNECTION_MANAGER_HPP
#define QUICFLOWCPP_CONNECTION_MANAGER_HPP

#include <unordered_map>
#include <msquic.h>
#include <memory>
#include <functional>


namespace quicflow {

namespace network {
class QuicConnection;

}

namespace manager {
#include "common/singleton.hpp"


class ConnectionManager : public Common::Singleton<ConnectionManager> {
public:
  ConnectionManager();

  void OnNewConnection(std::shared_ptr<network::QuicConnection>);

private:
  std::unordered_map<HQUIC, std::shared_ptr<network::QuicConnection>> connection_map_;

};

}
}


#endif  // QUICFLOWCPP_CONNECTION_MANAGER_HPP
