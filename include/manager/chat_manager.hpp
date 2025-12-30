//
// Created by 최진성 on 25. 12. 26..
//

#ifndef QUICFLOWCPP_CHAT_MANAGER_HPP
#define QUICFLOWCPP_CHAT_MANAGER_HPP

#include "common/singleton.hpp"

namespace quicflow {

namespace manager {
class ChatManager : public Common::Singleton<ChatManager> {
public:
  ChatManager();
};

}
}
#endif  // QUICFLOWCPP_CHAT_MANAGER_HPP