# QuicFlow-CPP (HTTP/3 / QUIC Chat Server Skeleton)

모바일 MMORPG 환경에서의 불안정한 네트워크(Wi‑Fi ↔ LTE 전환, 패킷 손실 등)를 가정한  
**HTTP/3(QUIC) 기반 고성능 채팅 서버** 포트폴리오 프로젝트의 C++ 테스트 서버 

## 기술 스택

- **언어**: C++20
- **네트워킹 코어**: Microsoft MsQuic (QUIC / HTTP/3)
- **라이브러리**
  - Boost.Asio (향후 MsQuic 이벤트 통합용, Phase 2 예정)
  - Boost.JSON (메시지 직렬화/역직렬화)
  - Boost.Multi_Index / Boost.Lockfree (세션/메시지 큐, 후속 단계)
- **빌드 시스템**: CMake + Ninja
- **컨테이너 환경**: Docker (Ubuntu 22.04, Clang + GCC)

> Note: Ubuntu 22.04 기본 저장소는 Boost 1.74를 제공합니다.  
> 요구사항은 1.81+ 이지만, Phase 1에서는 빌드 단순화를 위해 패키지 버전을 사용하고  
> 추후 필요 시 소스 빌드로 교체할 예정입니다.

## 디렉토리 구조 (Phase 1)

- `Dockerfile` : Ubuntu 22.04 기반 C++20/MsQuic/Boost 개발+실행 이미지를 정의
- `CMakeLists.txt` : 최상위 CMake 설정, `quicflow_echo_server` 실행 파일 생성
- `src/main.cpp` : MsQuic API RAII 스켈레톤 및 Echo 핸들러 자리 표시자
- `include/` : 향후 MsQuic 래퍼/세션 관리 헤더 파일 위치 (현재는 비어 있음)
- `cmake/` : `FindMsQuic.cmake` 등 커스텀 CMake 모듈을 위한 디렉토리 (현재는 비어 있음)

## Phase 1의 한계와 다음 단계

- 현재:
  - 실제 QUIC 리스너 바인딩, self-signed 인증서 로드, HTTP/3/WebTransport 프레이밍은 미구현입니다.
  - Echo 로직은 JSON 로그 출력 수준의 placeholder입니다.
- 다음 단계(예정):
  - MsQuic 리스너/스트림에 대한 완전한 RAII 래퍼 (`QuicConnection`, `QuicStream` 등) 구현
  - Boost.Asio `io_context`와 MsQuic 이벤트 루프 통합
  - Boost.Lockfree, Boost.Multi_Index를 활용한 세션/메시지 파이프라인 설계
  - WebTransport 호환 메시지 포맷 및 Connection Migration 시나리오 구현

