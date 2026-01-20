//
// Created by 최진성 on 26. 1. 14..
//

#ifndef QUICFLOWCPP_LOGGER_HPP
#define QUICFLOWCPP_LOGGER_HPP

#include <iostream>
#include <string>
#include <mutex>
#include <format> // C++20 필수 (없으면 fmt 라이브러리 사용 권장)

namespace common {
// [색상 코드] 콘솔 가독성을 위해 ANSI Color 사용
#define COLOR_RESET   "\033[0m"
#define COLOR_INFO    "\033[37m" // 흰색
#define COLOR_WARN    "\033[33m" // 노란색
#define COLOR_ERROR   "\033[31m" // 빨간색

class Logger {
public:
  // 인스턴스 생성 금지 (Static Class)
  Logger() = delete;

  // =========================================================
  // 1. Log (일반 정보)
  // =========================================================
  template<typename... Args>
  static void Log(std::format_string<Args...> fmt, Args&&... args) {
    Output(Level::Info, fmt, std::forward<Args>(args)...);
  }

  // =========================================================
  // 2. Warning (경고)
  // =========================================================
  template<typename... Args>
  static void Warning(std::format_string<Args...> fmt, Args&&... args) {
    Output(Level::Warning, fmt, std::forward<Args>(args)...);
  }

  // =========================================================
  // 3. Error (오류)
  // =========================================================
  template<typename... Args>
  static void Error(std::format_string<Args...> fmt, Args&&... args) {
    Output(Level::Error, fmt, std::forward<Args>(args)...);
  }

private:
  enum class Level { Info, Warning, Error };
  static std::mutex _mutex; // Thread-Safe를 위한 뮤텍스

  // 실제 출력을 담당하는 내부 함수
  template<typename... Args>
  static void Output(Level level, std::format_string<Args...> fmt, Args&&... args) {

    // 1. 메시지 포맷팅 (C++20 std::format)
    // 여기서 인자들을 문자열로 변환합니다.
    std::string message = std::format(fmt, std::forward<Args>(args)...);

#ifdef _DEBUG
    // -----------------------------------------------------
    // [DEBUG 모드] : 콘솔 출력 (Thread-Safe)
    // -----------------------------------------------------
    std::lock_guard<std::mutex> lock(_mutex);

    switch (level) {
      case Level::Info:
        std::cout << COLOR_INFO << "[LOG] " << message << COLOR_RESET << std::endl;
        break;
      case Level::Warning:
        std::cout << COLOR_WARN << "[WARN] " << message << COLOR_RESET << std::endl;
        break;
      case Level::Error:
        std::cerr << COLOR_ERROR << "[ERR] " << message << COLOR_RESET << std::endl;
        break;
    }
#else
    // -----------------------------------------------------
    // [RELEASE 모드] : 파일 로그 or 무시
    // -----------------------------------------------------
    // 성능을 위해 콘솔 출력은 제거하고, 파일에 쓰거나
    // 심각한 에러(Error 레벨)만 남기는 로직을 여기에 작성합니다.
    if (level == Level::Error) {
      // TODO: WriteToFile("error.log", message);
    }
#endif
  }
};
}
#endif  // QUICFLOWCPP_LOGGER_HPP
