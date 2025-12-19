FROM ubuntu:22.04

# -------------------------------
# QuicFlow-CPP Phase 1 Dockerfile
# -------------------------------
# Goal:
#   - C++20 development/runtime image for a QUIC (HTTP/3) chat/echo server
#   - Clang + GCC toolchains
#   - CMake/Ninja
#   - Boost (APT-provided, 1.74 on Ubuntu 22.04)
#   - MsQuic (attempt APT first; leave hooks for source build in later phases)
#
# Design notes (Why):
#   - For a portfolio project focused on low-level networking, a reproducible
#     toolchain is more important than a minimal image size at this stage.
#   - We keep everything in a single dev+run image to simplify iteration.
#   - MsQuic and Boost are installed via packages first to keep build times low;
#     later phases may replace these with source builds for version control.

ENV DEBIAN_FRONTEND=noninteractive

RUN sed -i 's/archive.ubuntu.com/mirror.kakao.com/g' /etc/apt/sources.list && \
    apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        tzdata \
        git \
        wget \
        curl \
        build-essential \
        ninja-build \
        cmake \
        pkg-config \
        gdb \
        clang \
        clang-tools \
        gcc \
        g++ \
        libboost-all-dev \
        libssl-dev \
        libnuma-dev \
        net-tools \
        iproute2 \
        iputils-ping \
        vim \
    && rm -rf /var/lib/apt/lists/*

# 2. MsQuic 빌드 및 설치 (가장 확실한 방법)
# Mac ARM64 호환성을 위해 소스에서 직접 빌드합니다.
WORKDIR /tmp
RUN git clone --recursive https://github.com/microsoft/msquic.git \
    && cd msquic \
    && mkdir build && cd build \
    && cmake -DQUIC_TLS=openssl -DQUIC_BUILD_TEST=OFF .. \
    && make -j$(nproc) \
    && make install \
    # 라이브러리 링크 갱신
    && ldconfig \
    # 임시 파일 삭제
    && cd /tmp && rm -rf msquic

# Optional: hooks for building MsQuic from source (Phase 2+)
# We intentionally keep this commented out to avoid slowing down Phase 1 builds.
# To enable later, uncomment and adjust versions/branches as needed.
#
# RUN git clone https://github.com/microsoft/msquic.git /opt/msquic && \
#     cd /opt/msquic && \
#     ./build.sh --config Debug && \
#     ./build.sh --config Release

# Working directory for the project sources.
WORKDIR /opt/quicflow_cpp

# Copy CMake project (Phase 1: minimal set; docker build context is project root)
COPY . /opt/quicflow_cpp

# Default compilers: prefer Clang, but GCC is also installed.
ENV CC=clang
ENV CXX=clang++

# Configure + build in a separate build directory.
# We do this at image build time so that `docker run` can immediately execute
# the server binary once Phase 1 main.cpp is in place.
RUN cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=${CC} \
    -DCMAKE_CXX_COMPILER=${CXX} && \
    cmake --build build

# Expose a placeholder port for future QUIC listener (e.g., 443 or 4433).
EXPOSE 4433/udp

# In Phase 1, the binary will be named `quicflow_echo_server`.
# If the build is not yet present, this CMD will fail fast, making
# misconfigurations visible early to the developer.
CMD ["/opt/quicflow_cpp/build/quicflow_echo_server"]


