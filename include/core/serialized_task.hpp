//
// Created by 최진성 on 26. 1. 2..
//

#ifndef QUICFLOWCPP_SERIALIZED_TASK_HPP
#define QUICFLOWCPP_SERIALIZED_TASK_HPP
#include <memory>
#include <thread>

#include "common/logger.hpp"
#include "serialized_object.hpp"

namespace quicflow {
namespace core {
using namespace common;
using Task = std::function<void()>;
class SerializedObject;

class SerializedTask : public std::enable_shared_from_this<SerializedTask>{
public:
  SerializedTask(std::shared_ptr<SerializedObject> thisObject, Task task) {
    thisObject_ = thisObject;
    func_ = task;
    Logger::Log("SerializedTask Created");
  }

  void Process() {
    func_();
  }

private:
  std::shared_ptr<SerializedObject> thisObject_;
  Task func_;
};
}
}

#endif  // QUICFLOWCPP_SERIALIZED_TASK_HPP
