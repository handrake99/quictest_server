#include "network/quic_api.hpp"

#include <iostream> // 필요시 로그 용도

namespace quicflow::network {

QuicApi::QuicApi()
{
#ifdef QUICFLOW_HAS_MSQUIC
    if (MsQuicOpen2(&api_) != QUIC_STATUS_SUCCESS) {
        // We do not throw here to keep Phase 1 sample usage minimal.
        // Instead, we leave the pointer null and let caller check.
        api_ = nullptr;
    }
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

