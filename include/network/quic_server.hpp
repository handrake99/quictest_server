#pragma once

// QuicFlow-CPP - QUIC Server Implementation
// Why: QUIC 서버의 생명주기(리스닝 시작/중지)와 클라이언트 연결 관리를
//      RAII 패턴으로 캡슐화합니다. 이 클래스는 QuicApi와 QuicConfigManager를
//      결합하여 실제 네트워크 서버를 구성합니다.

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

// Forward declarations
namespace quicflow {
namespace network {
class QuicApi;
class QuicConfigManager;
}  // namespace network
}  // namespace quicflow

#ifdef QUICFLOW_HAS_MSQUIC
extern "C" {
#include <msquic.h>
}
#endif

namespace quicflow {
namespace network {

// Callback type for new connection events.
// Why: std::function을 사용하여 유연한 콜백 바인딩을 지원합니다.
//      향후 Boost.Asio와 통합할 때 이 콜백을 Asio strand로 전달할 수 있습니다.
//
// Args:
//   connection: Newly accepted connection handle (HQUIC).
//   context: User-provided context pointer (can be used to pass QuicServer*).
#ifdef QUICFLOW_HAS_MSQUIC
using ConnectionCallback = std::function<void(HQUIC connection, void* context)>;
#else
using ConnectionCallback = std::function<void(void* connection, void* context)>;
#endif

// RAII wrapper for QUIC server listener.
// This class manages the lifecycle of a QUIC listener, including:
//  - Starting/stopping the listener on a UDP port
//  - Handling new connection events via callbacks
//  - Managing certificate configuration for TLS/QUIC handshake
//
// Usage example:
//   QuicApi api{};
//   QuicConfigManager config(api, {"h3"});
//   QuicServer server(api, config, 4433);
//   server.SetConnectionCallback([](HQUIC conn, void* ctx) {
//     // Handle new connection
//   });
//   if (server.Start()) {
//     // Server is listening
//   }
class QuicServer {
 public:
  // Constructs a QUIC server with the given configuration.
  // Why: 서버는 QuicApi와 QuicConfigManager에 의존하므로, 생성자에서
  //      이를 받아서 저장합니다. 포트 번호도 생성 시점에 지정하여
  //      명확한 설정을 보장합니다.
  //
  // Args:
  //   api: Reference to initialized QuicApi instance. Must outlive this object.
  //   config: Reference to initialized QuicConfigManager. Must outlive this object.
  //   port: UDP port number to listen on (default: 4433, standard QUIC port).
  //
  // Preconditions:
  //   - api.is_available() must return true.
  //   - config.is_valid() must return true.
  explicit QuicServer(const QuicApi& api,
                       const QuicConfigManager& config,
                       uint16_t port = 4433);

  // Non-copyable: listener handles are unique resources.
  QuicServer(const QuicServer&) = delete;
  QuicServer& operator=(const QuicServer&) = delete;

  // Movable: allows transferring ownership of listener handles.
  QuicServer(QuicServer&& other) noexcept;
  QuicServer& operator=(QuicServer&& other) noexcept;

  ~QuicServer();

  // Starts listening for incoming connections.
  // Why: 리스너를 시작하는 작업은 실패할 수 있으므로(포트 바인딩 실패 등),
  //      bool 반환값으로 성공/실패를 명확히 전달합니다.
  //
  // Returns:
  //   - true if listener started successfully.
  //   - false if start failed (check error_message() for details).
  bool Start();

  // Stops listening and closes the listener.
  // Why: 명시적인 Stop 메서드를 제공하여 서버를 안전하게 종료할 수 있습니다.
  //      소멸자에서도 자동으로 호출되지만, 명시적 호출이 더 명확합니다.
  void Stop() noexcept;

  // Returns true if the server is currently listening.
  bool is_listening() const noexcept { return is_listening_; }

  // Returns the port number this server is configured to listen on.
  uint16_t port() const noexcept { return port_; }

  // Sets a callback function to be invoked when a new connection is accepted.
  // Why: std::function을 사용하여 람다, 멤버 함수, 함수 포인터 등
  //      다양한 콜백 타입을 지원합니다.
  //
  // Args:
  //   callback: Function to call when a new connection is accepted.
  //             The connection handle (HQUIC) and context pointer are passed.
  void SetConnectionCallback(ConnectionCallback callback);

  // Returns a human-readable error message if Start() failed.
  const std::string& error_message() const noexcept { return error_message_; }

 private:
  // Static callback function for MsQuic listener events.
  // Why: MsQuic C API requires a C-style function pointer for callbacks.
  //      This static method bridges to the instance method via context pointer.
  //
  // Args:
  //   listener: Listener handle that received the event.
  //   context: User context (cast to QuicServer*).
  //   event: Event structure containing event type and data.
  //
  // Returns:
  //   QUIC_STATUS_SUCCESS if event was handled successfully.
#ifdef QUICFLOW_HAS_MSQUIC
  static QUIC_STATUS QUIC_API ListenerCallback(HQUIC listener,
                                               void* context,
                                               QUIC_LISTENER_EVENT* event);
#else
  static int ListenerCallback(void* listener, void* context, void* event);
#endif

  // Instance method that handles listener events.
  // Why: Static callback에서 인스턴스 메서드로 위임하여 멤버 변수에
  //      접근할 수 있게 합니다.
#ifdef QUICFLOW_HAS_MSQUIC
  QUIC_STATUS HandleListenerEvent(QUIC_LISTENER_EVENT* event);
#else
  int HandleListenerEvent(void* event);
#endif

  // Helper to clean up listener resources.
  void Cleanup() noexcept;

#ifdef QUICFLOW_HAS_MSQUIC
  const QUIC_API_TABLE* api_;
  const QuicConfigManager* config_;
  HQUIC listener_;
#else
  const void* api_;
  const void* config_;
  void* listener_;
#endif

  uint16_t port_;
  bool is_listening_;
  std::string error_message_;
  ConnectionCallback connection_callback_;
};

}  // namespace network
}  // namespace quicflow

