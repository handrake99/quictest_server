# QuicFlow-CPP (HTTP/3 / QUIC Chat Server Skeleton)

모바일 MMORPG 환경에서의 불안정한 네트워크(Wi‑Fi ↔ LTE 전환, 패킷 손실 등)를 가정한  
**HTTP/3(QUIC) 기반 고성능 채팅 서버** 포트폴리오 프로젝트의 C++ 서버 측 코드베이스입니다.

Phase 1의 목표는 다음과 같습니다.
- Ubuntu 22.04 기반 Docker 컨테이너 안에 C++20, MsQuic, Boost, CMake, Clang/GCC가 포함된 **개발+실행 통합 환경**을 구축
- MsQuic 초기화 및 향후 Echo Server로 확장 가능한 **엔트리포인트 스켈레톤** 구현

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

## 빌드 및 실행

### 1. Docker 이미지 빌드

```bash
cd /Users/handrake/work/quicflow_test
docker build -t quicflow-cpp:dev .
```

- Dockerfile은 이미지 빌드 시점에 이미 `cmake -S . -B build` 및 `cmake --build build` 를 수행합니다.
- MsQuic 라이브러리(`libmsquic`)를 찾지 못하면 경고를 출력하고, 바이너리는 실행 시 오류를 리턴하도록 설계되어 있습니다.

### 2. 컨테이너 내 실행

```bash
docker run --rm -it quicflow-cpp:dev
```

- 기본 CMD는 `build/quicflow_echo_server` 를 실행합니다.
- Phase 1에서는 실제 QUIC 리스너를 열지 않고, MsQuic 초기화 성공 여부와  
  Boost.JSON을 사용한 간단한 JSON 메시지를 로그로 출력합니다.

예상 출력(성공 시 예시):

```text
[QuicFlow] {"type":"echo_placeholder","message":"QuicFlow-CPP Phase 1 skeleton is running."}
```

MsQuic 초기화 실패 시:

```text
[QuicFlow] MsQuic API not available at runtime. The container may be missing libmsquic or QUICFLOW_HAS_MSQUIC was not defined at compile time.
```

이 경우:
- Dockerfile 내 `libmsquic` 패키지 설치 여부를 확인하거나,
- 향후 `msquic` 소스 빌드 경로를 활성화해 라이브러리를 제공해야 합니다.

## 아키텍처 의도 (Why)

- **RAII 기반 MsQuic 래퍼**
  - `src/main.cpp`의 `QuicApi`, `QuicListener` 클래스는 MsQuic 핸들을 안전하게 감싸는 **RAII 패턴의 출발점**입니다.
  - 장기 실행되는 MMORPG 채팅 서버에서 커넥션/리스너 리소스 누수는 치명적이므로,  
    초기 단계부터 수명 관리를 명시적으로 설계합니다.

- **MsQuic ↔ Boost.Asio 통합 포인트**
  - 현재는 단순한 placeholder이지만, `main()` 내부 주석에 MsQuic 콜백을  
    **lock-free 큐 → Asio strand** 로 전달하는 이벤트 파이프라인을 계획해 두었습니다.
  - 이를 통해, QUIC 이벤트 처리는 MsQuic 스레드 컨텍스트에서 최소한의 작업만 수행하고,  
    실제 게임/채팅 로직은 Asio 기반 스레드 풀에서 안전하게 처리할 수 있습니다.

- **WebTransport/Connection Migration 대비**
  - QUIC 리스너/커넥션을 별도 RAII 클래스로 분리할 계획을 주석으로 고정해 두어,  
    이후 CID 기반 세션 유지, WebTransport 스트림 관리 로직을 자연스럽게 확장할 수 있도록 합니다.

## Phase 1의 한계와 다음 단계

- 현재:
  - 실제 QUIC 리스너 바인딩, self-signed 인증서 로드, HTTP/3/WebTransport 프레이밍은 미구현입니다.
  - Echo 로직은 JSON 로그 출력 수준의 placeholder입니다.
- 다음 단계(예정):
  - MsQuic 리스너/스트림에 대한 완전한 RAII 래퍼 (`QuicConnection`, `QuicStream` 등) 구현
  - Boost.Asio `io_context`와 MsQuic 이벤트 루프 통합
  - Boost.Lockfree, Boost.Multi_Index를 활용한 세션/메시지 파이프라인 설계
  - WebTransport 호환 메시지 포맷 및 Connection Migration 시나리오 구현

