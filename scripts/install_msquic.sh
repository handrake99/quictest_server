#!/bin/bash
set -e

# MsQuic 설치 스크립트 (macOS용)
# Why: macOS에는 MsQuic 패키지가 없어서 소스 빌드가 필요합니다.

echo "[MsQuic] Installing dependencies..."
brew install openssl@3 cmake ninja

# MsQuic 소스 클론 및 빌드
MSQUIC_DIR="$HOME/.local/msquic"
if [ ! -d "$MSQUIC_DIR" ]; then
    echo "[MsQuic] Cloning repository..."
    git clone --recursive https://github.com/microsoft/msquic.git "$MSQUIC_DIR"
fi

cd "$MSQUIC_DIR"

echo "[MsQuic] Building (this may take 10-20 minutes)..."
# macOS용 빌드 (OpenSSL 사용)
# Note: QUIC_TLS is deprecated, use QUIC_TLS_LIB instead
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$(brew --prefix openssl@3)" \
    -DQUIC_TLS_LIB=openssl

cmake --build build --config Release

echo "[MsQuic] Build complete!"
echo "[MsQuic] Library location: $MSQUIC_DIR/build/bin/Release/libmsquic.dylib"
echo "[MsQuic] Headers location: $MSQUIC_DIR/src/inc"
