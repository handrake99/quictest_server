#include "network/quic_api.hpp"

#include <iostream> // 필요시 로그 용도

namespace quicflow::network {

QuicApi::QuicApi()
{
#ifdef QUICFLOW_HAS_MSQUIC
    QUIC_STATUS status = MsQuicOpen2(&api_);
    if (status != QUIC_STATUS_SUCCESS) {
        // We do not throw here to keep Phase 1 sample usage minimal.
        // Instead, we leave the pointer null and let caller check.
        std::cerr << "[QuicApi] MsQuicOpen2 failed with status: " << status << std::endl;
        api_ = nullptr;
    } else {
        std::cout << "[QuicApi] MsQuic API initialized successfully" << std::endl;
    }
#else
    std::cerr << "[QuicApi] QUICFLOW_HAS_MSQUIC is not defined at compile time" << std::endl;
#endif
}

QuicApi::~QuicApi()
{
#ifdef QUICFLOW_HAS_MSQUIC
    if (api_ != nullptr) {
        MsQuicClose(api_);
        api_ = nullptr;
    }
#endif
}

bool QuicApi::is_available() const noexcept
{
#ifdef QUICFLOW_HAS_MSQUIC
    return api_ != nullptr;
#else
    return false;
#endif
}

} // namespace quicflow::network

