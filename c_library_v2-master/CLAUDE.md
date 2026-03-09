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

## 라이브러리 아키텍처: Header-Only

- 모든 코드가 `.h` 파일에 `static inline` 또는 `MAVLINK_HELPER`로 정의됨
- 별도 `.c` 파일 없음 — `#include`하면 컴파일러가 직접 삽입
- 따라서 빌드 시 `-I` 경로만 잡아주면 됨 (라이브러리 컴파일 불필요)

---

## 메시지 구조 이해

- 모든 메시지(HEARTBEAT, COMMAND_LONG, GPS 등)는 **동일한 헤더 구조** 사용
- 메시지마다 달라지는 것은 **MSGID**와 **PAYLOAD** 뿐
- common dialect에 **총 231개** 메시지 정의됨 (`common/mavlink_msg_*.h`)

### 주요 MSGID

| MSGID | 이름 | payload 내용 |
|-------|------|-------------|
| 0 (0x00) | `HEARTBEAT` | 기체 타입, 상태 (9바이트) |
| 1 | `SYS_STATUS` | 배터리, 센서 상태 |
| 24 | `GPS_RAW_INT` | GPS 위치, 위성 수 |
| 30 | `ATTITUDE` | roll, pitch, yaw |
| 33 | `GLOBAL_POSITION_INT` | 위도, 경도, 고도 |
| 76 (0x4C) | `COMMAND_LONG` | 명령 전송 (이륙, 착륙 등) |
| 77 | `COMMAND_ACK` | 명령 응답 |
| 253 | `STATUSTEXT` | 텍스트 메시지 |

---

## 송신 내부 호출 체인 (코드 추적 완료)

```
mavlink_msg_heartbeat_pack()           ← payload만 msg에 복사
  └→ mavlink_finalize_message()        ← 래퍼 (mavlink_helpers.h:306)
       └→ mavlink_finalize_message_chan()    ← 채널에서 status 꺼냄 (:296)
            └→ mavlink_finalize_message_buffer()  ← ★ 실제 작업 (:227)
                 1. 헤더 필드 채움 (STX, LEN, SEQ++, SYSID, COMPID, MSGID)
                 2. CRC 계산: crc_calculate(헤더-STX제외) + payload + crc_extra
                 3. msg->checksum에 저장
                 4. 서명 (signing 설정된 경우만)

mavlink_msg_to_send_buffer(buf, &msg)  ← msg → 전송용 byte[] 직렬화 (mavlink_helpers.h:451)
  - 헤더 바이트 배열로 복사
  - payload 복사
  - msg->checksum에서 ck[0], ck[1] 복사 (mavlink_ck_a/b는 미사용, 레거시)
  - 서명 복사 (있는 경우)
```

### `_mav_put_uint32_t` 등 매크로 (protocol.h:145~176)
- payload 버퍼의 특정 오프셋에 값을 Little-endian으로 쓰는 매크로
- 플랫폼에 따라 3가지 분기: byte_swap (Big-endian), byte_copy (비정렬), 직접 대입 (x86)

---

## CRC 상세

- `mavlink_ck_a` / `mavlink_ck_b` (mavlink_types.h:161~162): payload 뒤 메모리에 CRC 저장하는 레거시 매크로
- 실제 전송 시에는 `msg->checksum` 필드에서 직접 쪼개서 사용 (mavlink_helpers.h:485~486)
- `crc_extra`: 메시지별 고유 상수 (HEARTBEAT=50). 송수신 측 메시지 구조 일치 검증용

---

## 서명 (Signing)

### 서명 활성화 조건 (mavlink_helpers.h:232)
```c
bool signing = (!mavlink1)
            && status->signing                                        // NULL이면 비활성
            && (status->signing->flags & MAVLINK_SIGNING_FLAG_SIGN_OUTGOING);
```
- 기본값: `status->signing == NULL` → 모든 메시지에서 서명 꺼짐
- HEARTBEAT라서 서명 안 되는 게 아님 — 채널 레벨 설정

### 서명 등록 방법
```c
mavlink_signing_t signing;
mavlink_signing_streams_t signing_streams;

memset(&signing, 0, sizeof(signing));
// 32바이트 비밀 키 (서버/클라이언트 동일해야 함)
uint8_t secret_key[32] = { 0x01, 0x02, ... , 0x20 };
memcpy(signing.secret_key, secret_key, 32);
signing.flags = MAVLINK_SIGNING_FLAG_SIGN_OUTGOING;
signing.link_id = 0;
signing.timestamp = (현재 마이크로초);

mavlink_status_t *status = mavlink_get_channel_status(MAVLINK_COMM_0);
status->signing = &signing;
status->signing_streams = &signing_streams;
```

### 서명 패킷 구조 (서명 시 +13바이트)
```
헤더(10) | PAYLOAD | CK_A CK_B | link_id(1) | timestamp(6) | signature(6)
```
- `incompat_flags`에 `0x01` 자동 세팅
- 서명 = SHA-256(secret_key + header + payload + CRC + link_id + timestamp)의 앞 6바이트

---

## 구현 현황

### 완료
- [x] HEARTBEAT 기반 UDP 서버-클라이언트 구현 (`server.c`, `client.c`, `Makefile`)
- [x] Wireshark 패킷 캡쳐 확인 (WSL → tcpdump → .pcap → Windows Wireshark)
- [x] HEARTBEAT 패킷 바이트 검증 완료: `fd0900007b0101000000...5360` (21바이트)

### 빌드 참고
```makefile
CFLAGS = -I./c_library_v2-master -Wall -Wno-address-of-packed-member
```
- `-Wno-address-of-packed-member`: 라이브러리 내부 packed struct 경고 무시 (MAVLink 프로젝트 공통)

### Wireshark 캡쳐 (WSL 환경)
```bash
sudo tcpdump -i lo -w /tmp/mavlink.pcap udp port 14550 &
# 서버/클라이언트 실행 후
cp /tmp/mavlink.pcap /mnt/c/Users/$(whoami)/Desktop/mavlink.pcap
```

---

## 작업 환경
- 집 PC / 연구실 PC 이동 작업 중
- 이 파일을 git으로 관리하여 양쪽 동기화
