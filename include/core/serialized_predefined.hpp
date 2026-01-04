//
// Created by 최진성 on 26. 1. 4..
//

#ifndef QUICFLOWCPP_SERIALIZED_PREDEFINED_HPP
#define QUICFLOWCPP_SERIALIZED_PREDEFINED_HPP

// -------------------------------------------------------------
// [헤더용] 선언 및 템플릿 구현 매크로
// -------------------------------------------------------------
// 원리:
// 1. private 함수는 정확한 인자 타입(__VA_ARGS__)으로 선언합니다.
// 2. public Async 함수는 '템플릿'으로 만들어서 어떤 인자든 받게 합니다.
// 3. 내부에서 decltype(*this)를 사용해 현재 클래스 이름을 자동으로 알아냅니다.
// -------------------------------------------------------------
#define DECLARE_ASYNC_FUNCTION(FuncName, ...) \
private: \
void FuncName(__VA_ARGS__); \
public: \
template<typename... Args> \
void FuncName##Async(Args&&... args) { \
using ThisClass = std::remove_reference_t<decltype(*this)>; \
this->SerializeAsync(&ThisClass::FuncName, std::forward<Args>(args)...); \
}

// -------------------------------------------------------------
// [CPP용] 정의 매크로 (이제 아주 단순해졌습니다!)
// -------------------------------------------------------------
// 변수명을 따로 분리해서 넣을 필요 없이, 그냥 함수 정의하듯이 쓰면 됩니다.
// -------------------------------------------------------------
#define DEFINE_ASYNC_FUNCTION(Class, FuncName, ...) \
void Class::FuncName(__VA_ARGS__)

#endif  // QUICFLOWCPP_SERIALIZED_PREDEFINED_HPP
