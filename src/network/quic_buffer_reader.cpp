//
// Created by 최진성 on 25. 12. 27..
//

#include "network/quic_buffer_reader.hpp"

bool QuicBufferReader::CopyDataFromBuffers(
        const QUIC_BUFFER* Buffers,
        uint32_t BufferCount,
        uint32_t Offset,
        void* Destination,
        uint32_t Length) {
  uint8_t* DestPtr = (uint8_t*)Destination;
  uint32_t RemainingCopy = Length;
  uint32_t CurrentOffset = 0;

  for (uint32_t i = 0; i < BufferCount; ++i) {
    uint32_t BufLen = Buffers[i].Length;

    // 아직 Offset(건너뛸 부분) 영역이라면
    if (CurrentOffset + BufLen <= Offset) {
      CurrentOffset += BufLen;
      continue;
    }

    // 복사 시작 위치 계산
    uint32_t StartInBuf = (Offset > CurrentOffset) ? (Offset - CurrentOffset) : 0;
    uint32_t CopySize = BufLen - StartInBuf;

    if (CopySize > RemainingCopy) {
      CopySize = RemainingCopy;
    }

    memcpy(DestPtr, Buffers[i].Buffer + StartInBuf, CopySize);

    DestPtr += CopySize;
    RemainingCopy -= CopySize;
    CurrentOffset += BufLen; // 다음 버퍼를 위해 오프셋 증가

    if (RemainingCopy == 0) {
      return true; // 다 복사함
    }
  }

  return false; // 복사할 데이터가 부족함 (Error)
}

// 2. 전체 버퍼 크기 계산
uint32_t QuicBufferReader::GetTotalLength(const QUIC_BUFFER* Buffers, uint32_t BufferCount) {
    uint32_t Total = 0;
    for (uint32_t i = 0; i < BufferCount; ++i) {
      Total += Buffers[i].Length;
    }
  return Total;
}

// 3. [핵심] 메시지 파싱 함수
// 리턴값: 성공 시 true, 데이터 부족 시 false (다음 이벤트를 기다려야 함)
// OutputString: 결과가 담길 변수
bool QuicBufferReader::TryParseStringMessage(
    const QUIC_BUFFER* Buffers,
    uint32_t BufferCount,
    std::string& OutputString)
{

  uint32_t TotalBytes = GetTotalLength(Buffers, BufferCount);

  // A. 헤더(4byte)조차 다 안 왔으면 리턴
  if (TotalBytes< 4) {
    return false;
  }

  // B. 길이(Header) 추출
  uint8_t HeaderBytes[4];
  // 흩어진 버퍼에서 안전하게 4바이트를 읽어옴
  CopyDataFromBuffers(Buffers, BufferCount, 0, HeaderBytes, 4);

  // ★ Little Endian 디코딩 (강제) ★
  uint32_t BodyLength =
      ((uint32_t)HeaderBytes[0])       |
      ((uint32_t)HeaderBytes[1] << 8)  |
      ((uint32_t)HeaderBytes[2] << 16) |
      ((uint32_t)HeaderBytes[3] << 24);

  // [검증 1] 보안 체크: 메시지가 너무 크면 거부 (메모리 공격 방지)
  if (BodyLength > MAX_MESSAGE_SIZE) {
    printf("[Error] Message size too large: %u\n", BodyLength);
    // 필요 시 여기서 연결을 끊는 로직 추가 (StreamShutdown 등)
    return false;
  }


  // [검증 2] 데이터 완전성 체크: 헤더(4) + 본문(Length)만큼 데이터가 다 왔는가?
  if (TotalBytes < 4 + BodyLength) {
    // 아직 패킷이 덜 도착함. (TCP/QUIC의 스트림 특성)
    // 다음 RECEIVE 이벤트 때 데이터가 더 쌓이면 그때 처리해야 함.
    return false;
  }

  // C. 본문(String) 추출
  OutputString.resize(BodyLength); // string 크기 확보

  // 헤더 4바이트 건너뛰고(Offset=4), BodyLength만큼 복사
  CopyDataFromBuffers(Buffers, BufferCount, 4, &OutputString[0], BodyLength);

  std::cout << "[QuicStream] Receive Buffer Success : " << BodyLength << "!" << std::endl;

  return true; // 파싱 성공!
}