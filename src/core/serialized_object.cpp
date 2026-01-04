//
// Created by 최진성 on 26. 1. 2..
//

#include "core/serialized_object.hpp"
#include "core/serialized_task.hpp"

using namespace quicflow::core;

SerializedObject::SerializedObject() {
  count_ = 0;
  isRunning_ = 0;
  isDestory_ = false;
}

void SerializedObject::Serialize(std::shared_ptr<SerializedTask> newTask) {
  long count = 0;
  if (count_.compare_exchange_strong(count, 1)) {
    newTask->Process();

    if (count_.fetch_add(1) > 0) {
      // run
      RunQueue();
    }
    return;
  }else {
    count = Enqueue(newTask);
    if (count == 1) {
      //run
      RunQueue();
    }
  }
}

/*template<typename Func, typename... Args>
void SerializedObject::SerializeAsync(Func func, Args&&... args) {
  auto self = shared_from_this();
  auto task = [self, func, args...]() mutable {
    (self.get()->*func)(args...);
  };
  auto serializedTask = std::make_shared<SerializedTask>(task);
  serializedTask->SerializeAsync(task);
}*/

void SerializedObject::RunQueue() {
  std::shared_ptr<SerializedTask> curTask ;

  do {
    if (isDestory_) {
      return;
    }

    curTask = Dequeue();
    if (curTask == nullptr) {
      //error
      return;
    }
    curTask->Process();
  }while (count_.fetch_sub(1) > 0);
}
long SerializedObject::Enqueue(std::shared_ptr<SerializedTask> task) {
  queue_.enqueue(task);
  return count_.fetch_add(1);
}
std::shared_ptr<SerializedTask> SerializedObject::Dequeue() {
  std::shared_ptr<SerializedTask> retTask;
  while (queue_.try_dequeue(retTask) == false);
  return retTask;
}
