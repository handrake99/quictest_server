//
// Created by 최진성 on 25. 12. 29..
//

#ifndef QUICFLOWCPP_QUIC_PROTOCOL_HPP
#define QUICFLOWCPP_QUIC_PROTOCOL_HPP

#include <string>
#include <vector>
#include <nlohmann/json.hpp> // nlohmann/json 헤더 포함

using json = nlohmann::json;

// 1. 데이터를 담을 구조체 정의
struct ChatProtocol{
  std::string Type;
  uint32_t MessageId;
  std::string UserID;
  std::string Message;
  std::time_t Timestamp; // C++은 날짜 타입이 복잡하므로 문자열로 주고받는 게 정신건강에 좋습니다.
};

// 2. [핵심] JSON <-> 구조체 자동 변환 매크로
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChatProtocol, Type, UserID, Message, Timestamp);

#endif  // QUICFLOWCPP_QUIC_PROTOCOL_HPP
