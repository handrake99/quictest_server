//
// Created by 최진성 on 25. 12. 20..
//

#include "manager/connection_manager.hpp"

#include <iostream>
#include <memory>

#include "network/quic_connection.hpp"
#include "network/quic_protocol.hpp"

namespace quicflow {
namespace manager {
using namespace network;

ConnectionManager::ConnectionManager() {
}

// 새로운 connection 이 들어왔을때 처리
void ConnectionManager::OnNewConnection(std::shared_ptr<QuicConnection> connection) {
  std::cout << "[ConnectionManager] OnNewConnection Called (" << connection->connection() << ")" << std::endl;
  auto key = connection->connection();

  if (connection_map_.contains(key) == true) {
    // 기존의 존재하는경우 새로운 걸로 교체하고 기존꺼는 버린다.
    auto oldConnection = connection_map_.at(key) ;
    std::cerr << "[DEBUG][F] Already existed connection(" << connection->connection()<< ")" << std::endl;
    connection_map_.erase(key);
  }
  connection_map_.insert(std::make_pair(key, connection));
}

// connection close 처리
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

// chatting message를 받아 다른 유저에게 broadcasting 한다
void ConnectionManager::OnReceiveChatMessage(std::shared_ptr<network::QuicConnection> connection, std::string& jsonMessage) {
  auto key = connection->connection();

  if (connection_map_.contains(key) == false) {
    std::cerr << "[DEBUG][F] No connection(" << connection->connection()<< ")" << std::endl;
    return;
  }

  // message deserialize
  try
  {
    // 1. 파싱 (문자열 -> json 객체)
    json j = json::parse(jsonMessage);
    // 2. 역직렬화 (json 객체 -> 구조체)
    // C#의 JsonSerializer.Deserialize<ChatPayload>와 똑같습니다.
    auto parsedData = j.get<ChatProtocol>();

    // 3. 사용
    std::cout << "[Deserialized] Type: " << parsedData.Type << std::endl;
    std::cout << "[Deserialized] MessageId: " << parsedData.MessageId<< std::endl;
    std::cout << "[Deserialized] User: " << parsedData.UserID << std::endl;
    std::cout << "[Deserialized] Msg: "  << parsedData.Message << std::endl;
    std::cout << "[Deserialized] Time: " << parsedData.Timestamp << std::endl;

    for (auto curIter = connection_map_.begin(); curIter != connection_map_.end(); ++curIter) {
      auto curConnection = curIter->second;
      curConnection->SendChatMessageAsync(parsedData.Message);
    }
  } catch (json::parse_error& e) {
    // JSON 형식이 깨져서 왔을 때
    std::cerr << "[Error] JSON Parse failed: " << e.what() << std::endl;
  } catch (json::type_error& e) {
    // 필수 필드가 없거나 타입이 다를 때 (예: 숫자가 와야 하는데 문자열이 옴)
    std::cerr << "[Error] Data type mismatch: " << e.what() << std::endl;
  }
}

}
}