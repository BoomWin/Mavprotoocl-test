# CLAUDE.md — MAVLink UDP 통신 프로젝트

## 프로젝트 목표
MAVLink v2 프로토콜을 사용한 UDP 기반 Client-Server 통신 구현.
- 라이브러리: `c_library_v2-master` (MAVLink C library v2)
- 전송 계층: UDP (POSIX socket)
- 언어: C

---

## 라이브러리 핵심 파일

| 파일 | 역할 |
|------|------|
| `mavlink_types.h` | 핵심 구조체 (`mavlink_message_t`, `mavlink_status_t`) |
| `mavlink_helpers.h` | 파싱/직렬화 핵심 함수 |
| `protocol.h` | 프로토콜 상수 (STX, 오프셋, 패킷 크기 등) |
| `checksum.h` | CRC-16 MCRF4XX |
| `common/mavlink.h` | 전체 common 다이얼렉트 단일 진입점 (include 권장) |

---

## 송신 흐름

1. `mavlink_msg_<name>_pack(sysid, compid, &msg, ...)` — 메시지 구성
2. `mavlink_msg_to_send_buffer(buf, &msg)` — raw byte 직렬화 (반환값: 전송 바이트 수)
3. `sendto()` — UDP 전송

## 수신 흐름

1. `recvfrom()` — UDP 수신
2. 수신 버퍼를 바이트 단위 루프
3. `mavlink_parse_char(chan, byte, &msg, &status)` — 반환 1이면 패킷 완성
4. `msg.msgid` switch → `mavlink_msg_<name>_decode(&msg, &struct)` — 디코딩

---

## 설계 결정사항

### sysid / compid 관례
- GCS(서버): `sysid=255, compid=190`
- Vehicle(클라이언트): `sysid=1, compid=1`
- 서버에서 클라이언트 식별 시 `msg.sysid` 사용

### 채널(Channel)
- 논리 연결 1개 = `mavlink_channel_t` 1개 (MAVLINK_COMM_0 ~ 3)
- 다중 클라이언트 서버: 클라이언트(IP:Port)별로 `mavlink_status_t` 분리 유지
- 이유: SEQ 카운터 및 파서 상태가 연결마다 독립적이어야 함

### include 방식
```c
#include "common/mavlink.h"  // 단일 dialect만 include (중복 정의 방지)
```

### 버퍼 크기
```c
uint8_t buf[MAVLINK_MAX_PACKET_LEN];  // 최대 280바이트
```

---

## 주의사항

| 이슈 | 해결 |
|------|------|
| 다중 클라이언트 | 클라이언트별 `mavlink_status_t` 별도 관리 |
| 멀티스레드 | 채널별 뮤텍스 또는 스레드 분리 |
| HEARTBEAT 주기 | 1Hz 권장 (GCS 타임아웃 3~5초) |
| MAVLink v1 혼용 | `status.flags & MAVLINK_STATUS_FLAG_IN_MAVLINK1` 로 감지 |

---

## MAVLink v2 패킷 구조

```
STX(1) | LEN(1) | INCOMPAT(1) | COMPAT(1) | SEQ(1) | SYSID(1) | COMPID(1) | MSGID(3) | PAYLOAD(0~255) | CK_A | CK_B
```
- STX = `0xFD`
- CRC: X25 CRC-16, header(STX 제외) + payload + crc_extra

---

## 작업 환경
- 집 PC / 연구실 PC 이동 작업 중
- 이 파일을 git으로 관리하여 양쪽 동기화
