#include "network/quic_api.hpp"

#include <iostream> // 필요시 로그 용도

namespace quicflow::network {

QuicApi::QuicApi()
{
    QUIC_STATUS status = MsQuicOpen2(&api_);
    if (status != QUIC_STATUS_SUCCESS) {
        // We do not throw here to keep Phase 1 sample usage minimal.
        // Instead, we leave the pointer null and let caller check.
        std::cerr << "[QuicApi] MsQuicOpen2 failed with status: " << status << std::endl;
        api_ = nullptr;
    } else {
        std::cout << "[QuicApi] MsQuic API initialized successfully" << std::endl;
    }
}

QuicApi::~QuicApi()
{
    if (api_ != nullptr) {
        //MsQuicClose(api_);
        api_ = nullptr;
    }
}

bool QuicApi::is_available() const noexcept
{
    return api_ != nullptr;
}

} // namespace quicflow::network

