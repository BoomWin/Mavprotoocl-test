#ifndef MAVLINK_PQC_H
#define MAVLINK_PQC_H

#include <cstdint>
#include <netinet/in.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include "./c_library_v2-master/common/mavlink.h"

#define MSG_TYPE_PQC_PUBKEY_FRAG        0x0010
#define MSG_TYPE_PQC_CIPHERTEXT_FRAG    0x0011
#define MSG_TYPE_PQC_KEX_DONE           0x0012

// Extension 페이로드 헤더 | 4(헤더) |  249  
// [0] fragment_id (현재 조각 번호), [1] total_fragments(전체 조각 수), [2~3] total_data_len (원본 데이터 전체 길이, 1byte로는 표현이 안돼서 2로 잡음)
#define FRAG_DATA_MAX                   245

// 분할 전송 함수
// data를 245 바이트씩 쪼개서 V2_EXTENSION으로 전송
static inline int pqc_send_fragmented(
    int sock,
    struct sockaddr_in *dest_addr,
    uint8_t sysid,
    uint8_t compid,
    uint8_t target_sys,
    uint8_t target_comp,
    uint16_t message_type,
    const uint8_t *data, uint16_t data_len) 
{
    // 나누어떨어지지 않을때를 대비해서 
    uint8_t total_frags = (data_len + FRAG_DATA_MAX - 1) / FRAG_DATA_MAX;

    for (uint8_t i = 0; i < total_frags; i++) {
        uint16_t offset = i * FRAG_DATA_MAX;
        uint16_t chunk_len = data_len - offset;
        if (chunk_len > FRAG_DATA_MAX)
            chunk_len = FRAG_DATA_MAX;

        uint8_t ext_payload[249];
        memset(ext_payload, 0, sizeof(ext_payload));
    }
}
#endif