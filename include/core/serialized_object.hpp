//
// Created by 최진성 on 26. 1. 2..
//

#ifndef QUICFLOWCPP_SERIALIZED_OBJECT_HPP
#define QUICFLOWCPP_SERIALIZED_OBJECT_HPP
#include "concurrentqueue.h"
#include "serialized_task.hpp"

namespace quicflow {
namespace core {
class SerializedTask;

class SerializedObject : public std::enable_shared_from_this<SerializedObject>{
public:
  SerializedObject();

  void Serialize(std::shared_ptr<SerializedTask> task);
  void RunQueue();
  long Enqueue(std::shared_ptr<SerializedTask> task);
  std::shared_ptr<SerializedTask> Dequeue();

  //template<typename Func, typename... Args>
  //void SerializeAsync(Func func, Args&&... args) ;
  template<typename TargetClass, typename... FuncArgs, typename... Args>
  void SerializeAsync(void (TargetClass::*func)(FuncArgs...), Args&&... args){
    auto self = shared_from_this();
    auto task = [self, func, args...]() mutable {
      TargetClass* derivedPtr = static_cast<TargetClass*>(self.get());
      (derivedPtr->*func)(args...);
    };
    auto serializedTask = std::make_shared<SerializedTask>(self, task);
    Serialize(serializedTask);
  }

private:
  moodycamel::ConcurrentQueue<std::shared_ptr<SerializedTask>> queue_;

  std::atomic<long> count_;
  std::atomic<long> isRunning_;
  bool isDestory_;

};
}
}
#endif  // QUICFLOWCPP_SERIALIZED_OBJECT_HPP
