//
// Created by 최진성 on 25. 12. 27..
//

#ifndef QUICFLOWCPP_QUIC_BUFFER_READER_HPP
#define QUICFLOWCPP_QUIC_BUFFER_READER_HPP

#include <msquic.h>
#include <string>
#include <vector>
#include <iostream>

// 보안을 위한 최대 허용 메시지 크기 (예: 1MB)
// 해커가 40억 바이트(4GB)라고 헤더를 조작해 보내면 메모리가 터지므로 제한 필수
const uint32_t MAX_MESSAGE_SIZE = 1024 * 1024;

// 통신을 위한 Buffer Reader
// Header(Length) : 4byte
// Body : Length byte
class QuicBufferReader {
public:
  static bool TryParseStringMessage(
      const QUIC_BUFFER* Buffers,
      uint32_t BufferCount,
      std::string& OutputString);
private:
  static bool CopyDataFromBuffers(
          const QUIC_BUFFER* Buffers,
          uint32_t BufferCount,
          uint32_t Offset,
          void* Destination,
          uint32_t Length);
  static uint32_t GetTotalLength(const QUIC_BUFFER* Buffers, uint32_t BufferCount);
};

#endif  // QUICFLOWCPP_QUIC_BUFFER_READER_HPP
