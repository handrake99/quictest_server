//
// Created by 최진성 on 25. 12. 19..
//

#include <memory>
#include <mutex>

namespace Common {
  template <typename T>
  class Singleton {
  public:
    static T& GetInstance() {
      static T instance;
      return instance;
    }

    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

  protected:
    Singleton() {}
    virtual ~Singleton() {}
  };
}

