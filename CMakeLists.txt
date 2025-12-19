cmake_minimum_required(VERSION 3.20)

# ----------------------------------------
# QuicFlow-CPP - Phase 1 CMake Top Level
# ----------------------------------------
# Why this structure:
#   - Keep a single translation unit (main.cpp) in Phase 1 to focus on
#     toolchain, MsQuic wiring, and Docker integration.
#   - Make it trivial to split into libraries (core/quic/session/chat)
#     in later phases without changing external build commands.

project(QuicFlowCPP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Configuring QuicFlow-CPP with CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")

# Prefer Clang in the container, but allow overriding.
if(NOT CMAKE_CXX_COMPILER)
    message(STATUS "CMAKE_CXX_COMPILER not set explicitly, relying on environment or CMake default.")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS "Using Clang-family compiler: ${CMAKE_CXX_COMPILER}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "Using GCC: ${CMAKE_CXX_COMPILER}")
else()
    message(WARNING "Using unsupported/untested compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

set(COMMON_WARN_FLAGS "-Wall" "-Wextra" "-Wpedantic")
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND COMMON_WARN_FLAGS "-Wconversion" "-Wimplicit-fallthrough")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND COMMON_WARN_FLAGS "-Wconversion" "-Wimplicit-fallthrough=2")
endif()

add_compile_options(${COMMON_WARN_FLAGS})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(QUICFLOW_DEBUG)
endif()

# Allow custom CMake modules (e.g., FindMsQuic.cmake) in later phases.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# -------------------------
# Dependencies: Boost, MsQuic
# -------------------------

# Boost:
# - Phase 1: header-only and basic libs, focusing on json/asio usage.
# - Ubuntu 22.04 provides Boost 1.74; the design targets 1.81+,
#   so advanced features should be guarded with version checks later.
find_package(Boost REQUIRED COMPONENTS json)

# MsQuic:
# There is no standard CMake config on all distros. For Phase 1 we try a
# minimal find_library approach against libmsquic. This will be replaced
# with a dedicated FindMsQuic.cmake or an imported target in later phases.
#
# Why this order:
#   1. Try system-wide installation (e.g., via package manager)
#   2. Fall back to local build in $HOME/.local/msquic (macOS source build)
#   3. If neither found, build continues but QUICFLOW_HAS_MSQUIC won't be defined
find_library(MSQUIC_LIBRARY msquic
    PATHS
        "$ENV{HOME}/.local/msquic/build/bin/Release"
        "$ENV{HOME}/.local/msquic/build/bin/Debug"
        /usr/local/lib
        /opt/homebrew/lib
)

# Try to find MsQuic headers
# On macOS, after source build, headers are in $HOME/.local/msquic/src/inc
find_path(MSQUIC_INCLUDE_DIR msquic.h
    PATHS
        "$ENV{HOME}/.local/msquic/src/inc"
        /usr/local/include
        /opt/homebrew/include
)

if(MSQUIC_LIBRARY AND MSQUIC_INCLUDE_DIR)
    message(STATUS "Found MsQuic library: ${MSQUIC_LIBRARY}")
    message(STATUS "Found MsQuic headers: ${MSQUIC_INCLUDE_DIR}")
else()
    message(WARNING "MsQuic library or headers not found. Phase 1 build will still succeed,")
    message(WARNING "but any code that directly calls MsQuic APIs must be guarded or stubbed.")
    message(WARNING "To install MsQuic on macOS, run: ./scripts/install_msquic.sh")
endif()

# -------------------------
# Targets
# -------------------------

add_executable(quicflow_echo_server
    src/main.cpp
    src/network/quic_api.cpp
)

target_include_directories(quicflow_echo_server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIRS}
)

# Add MsQuic include directory if found
if(MSQUIC_INCLUDE_DIR)
    target_include_directories(quicflow_echo_server
        PRIVATE
            ${MSQUIC_INCLUDE_DIR}
    )
endif()

target_link_libraries(quicflow_echo_server
    PRIVATE
        Boost::json
)

if(MSQUIC_LIBRARY AND MSQUIC_INCLUDE_DIR)
    target_link_libraries(quicflow_echo_server PRIVATE ${MSQUIC_LIBRARY})
    target_compile_definitions(quicflow_echo_server PRIVATE QUICFLOW_HAS_MSQUIC)
    message(STATUS "MsQuic support enabled (QUICFLOW_HAS_MSQUIC defined)")
else()
    message(STATUS "MsQuic support disabled (library or headers not found)")
endif()

# Design note:
#   - We deliberately keep main.cpp as the only source here. In later phases,
#     we will introduce libraries such as:
#       quicflow_core (RAII wrappers around MsQuic)
#       quicflow_transport (HTTP/3 / WebTransport framing)
#       quicflow_chat (session management, lock-free queues)
    #     and link them into this executable without changing the Docker UX.


