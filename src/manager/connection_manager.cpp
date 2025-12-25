//
// Created by 최진성 on 25. 12. 20..
//

#include "manager/connection_manager.hpp"

#include <iostream>
#include <memory>

#include "network/quic_connection.hpp"

namespace quicflow {
namespace manager {
using namespace network;

ConnectionManager::ConnectionManager() {

}

void ConnectionManager::OnNewConnection(std::shared_ptr<QuicConnection> connection) {
  std::clog << "[DEBUG][F] OnNewConnection Called (" << connection->connection() << ")" << std::endl;
  auto key = connection->connection();

  if (connection_map_.contains(key) == true) {
    // 기존의 존재하는경우 새로운 걸로 교체하고 기존꺼는 버린다.
    auto oldConnection = connection_map_.at(key) ;
    std::cerr << "[DEBUG][F] Already existed connection(" << connection->connection()<< ")" << std::endl;
    connection_map_.erase(key);
  }
  connection_map_.insert(std::make_pair(key, connection));
}

void ConnectionManager::OnCloseConnection(std::shared_ptr<QuicConnection> connection) {
  std::clog << "[DEBUG][F] OnCloseConnection Called (" << connection->connection()<< ")" << std::endl;
  auto key = connection->connection();
  if (connection_map_.contains(key) == false) {
    std::cerr << "[DEBUG][F] no connection(" << connection->connection()<< ")" << std::endl;
    return;
  }
  std::cerr << "[DEBUG][F] Erase connection(" << connection->connection()<< ")" << std::endl;
  connection->CloseConnection();
  connection_map_.erase(key);
}

}
}
