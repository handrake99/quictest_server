#pragma once

// QuicFlow-CPP - MsQuic API RAII wrapper
// Why: 중앙 집중 MsQuic 초기화/해제를 한 곳에서 책임지기 위해 분리.
//      이렇게 하면 나중에 QuicListener, QuicConnection 등이 모두
//      같은 QuicApi 인스턴스를 공유할 수 있어 메모리 효율적입니다.

#ifdef QUICFLOW_HAS_MSQUIC
extern "C" {
#include <msquic.h>
}
#endif

namespace quicflow::network {

class QuicApi {
public:
    QuicApi();
    QuicApi(const QuicApi&)            = delete;
    QuicApi& operator=(const QuicApi&) = delete;
    ~QuicApi();

#ifdef QUICFLOW_HAS_MSQUIC
    const QUIC_API_TABLE* native() const noexcept { return api_; }
#else
    const void* native() const noexcept { return nullptr; }
#endif

    bool is_available() const noexcept;

private:
#ifdef QUICFLOW_HAS_MSQUIC
    const QUIC_API_TABLE* api_{nullptr};
#else
    void* api_{nullptr};
#endif
};

} // namespace quicflow::network

