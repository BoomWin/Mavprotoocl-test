// 암호화 구현을 위해서 추가
#include "c_library_v2-master/minimal/mavlink_msg_heartbeat.h"
#include "mavlink_aes.h"

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "./c_library_v2-master/common/mavlink.h"


#define SERVER_IP   "127.0.0.1" // 자신 IP
#define SERVER_PORT 14550   // GCS에서 해당 포트를 표준처럼 디폴트로 사용한다고함

// 서명 콜백 추가
static int accept_unsigned(const mavlink_status_t *status, uint32_t msgid) {
    // 서명 없는 패킷 허용.
    return 1;
}

int main() {
    // 1) UDP 소켓 생성
    // ex : tcp = SCOK_STREAM, udp = SOCK_DGRAM
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("소켓 생성 실패...\n");
        return 1;
    }

    // 2) 서버 주소 설정
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // sign 기능 추가하기 위해서 구조체 선언
    mavlink_signing_t signing;
    mavlink_signing_streams_t signing_streams;

    printf("[CLIENT] Vehicle (sysid = 1) 시작, 서버 %s:%d로 HEARTBEAT 전송\n", SERVER_IP, SERVER_PORT);


    // 비밀 키 설정 (32 바이트 - 양쪽이 동일한 키를 사용해야 함)
    memset(&signing, 0, sizeof(signing));
    // 현재 임시 키
    uint8_t secret_key[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
    };
    memcpy(signing.secret_key, secret_key, 32);
    
    // 플래그, link_id, 타임스탬프 설정
    // 사용하기 위해서 해당 플래그 필요함.
    signing.flags = MAVLINK_SIGNING_FLAG_SIGN_OUTGOING;
    signing.link_id = 0;
    signing.accept_unsigned_callback = accept_unsigned;

    // 채널의 status에 등록
    mavlink_status_t *status = mavlink_get_channel_status(MAVLINK_COMM_0);
    status->signing = &signing;
    status->signing_streams = &signing_streams;


    // 3) 메인 루프 : 1초마다 HEARTBEAT 송신 + 서버 응답 수신
    while (1) {
        // 송신 부분
        mavlink_message_t msg;
        uint8_t buf[MAVLINK_MAX_PACKET_LEN];

        // 서명 생성에 사용될 타임 스탬프 값
        struct timeval tv_for_sign;
        gettimeofday(&tv_for_sign, NULL);
        signing.timestamp = (uint64_t)tv_for_sign.tv_sec * 1000000ULL + tv_for_sign.tv_usec;

        /* ===================  기존 평문 전송 코드 =================== */
        // HEARTBEAT 패킹 : sysid=1, compid=1 (Vehicle)
        // mavlink_msg_heartbeat_pack(
        //     1,                      // system_id (Vehicle)
        //     1,                      // component_id
        //     &msg,
        //     MAV_TYPE_QUADROTOR,      // type: 쿼드 콥터
        //     MAV_AUTOPILOT_GENERIC,   // autopilot: 일반 자동조종 장치
        //     MAV_MODE_FLAG_CUSTOM_MODE_ENABLED, // base_mode: 사용자 모드 활성화
        //     0,                        // custom_mode: 사용자 모드 0
        //     MAV_STATE_ACTIVE          // system_status: 활성 상태
        // );

        /* =============== 암호화 HEARTBEAT (V2_EXTENSION) =========== */
        // haertbeat 구조체를 직접 채움
        mavlink_heartbeat_t hb;
        memset(&hb, 0, sizeof(hb));
        hb.custom_mode      = 0;
        hb.type             = MAV_TYPE_QUADROTOR;
        hb.autopilot        = MAV_AUTOPILOT_GENERIC;
        hb.base_mode        = MAV_MODE_FLAG_CUSTOM_MODE_ENABLED;
        hb.system_status    = MAV_STATE_ACTIVE;
        hb.mavlink_version  = 3;

        // AES-128-CBC 암호화
        uint8_t iv[16];
        uint8_t ciphertext[32];
        int cipher_len = 0;

        if (mavlink_aes_encrypt((uint8_t *)&hb, MAVLINK_MSG_ID_HEARTBEAT_LEN, ciphertext, &cipher_len, iv) != 0) {
            printf("[CLIENT] 암호화 실패..!\n");
            sleep(1);
            continue;           
        }

        // V2_EXTENSION payload 조립
        uint8_t ext_payload[249];
        memset(ext_payload, 0, sizeof(ext_payload));
        // 0번째 페이로드 데이터는 원래 기존 하트비트의 길이 정보
        ext_payload[0] = MAVLINK_MSG_ID_HEARTBEAT_LEN;
        // 1~16까지는 iv 값
        memcpy(&ext_payload[1], iv, 16);
        // 17~32 암호문
        memcpy(&ext_payload[17], ciphertext, cipher_len); 

        // V2_EXTENSION 패킹
        mavlink_msg_v2_extension_pack(
            1,                              // sysid = 1,
            1,                              // compid = 1,
            &msg,                            // msg
            0,                              // target_network
            255,                            // target_system
            190,                            // target_component
            MSG_TYPE_ENCRYPTED_HEARTBEAT,   // message_type = 0x0001
            ext_payload
        );


        // 직렬화
        uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);

        // UDP 전송
        sendto(sock, buf, len, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
        printf("[CLIENT] 암호화 HEARTBEAT 전송 (seq=%d, %d bytes)\n", msg.seq, len);

        // 수신 부분
        struct timeval tv = {0, 100000}; // 100ms timeout
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        uint8_t recv_buf[MAVLINK_MAX_PACKET_LEN];
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        ssize_t recv_len = recvfrom(sock, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&from_addr, &from_len);

        /* 기존 HEARTBEAT 수신 구현 */
        // if (recv_len > 0) {
        //     // 바이트 단위 파싱
        //     mavlink_message_t recv_msg;
        //     mavlink_status_t status;
        //     for (int i = 0; i < recv_len; i++) {
        //         if (mavlink_parse_char(MAVLINK_COMM_0, recv_buf[i], &recv_msg, &status)) {
        //             if (recv_msg.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
        //                 mavlink_heartbeat_t hb;
        //                 mavlink_msg_heartbeat_decode(&recv_msg, &hb);
        //                 printf("[CLIENT] 서버 HEART BEAT 수신 ... (sysid=%d, type=%d)\n", 
        //                         recv_msg.sysid, hb.type);
        //             }
        //         }
        //     }
        // }

        /* 복호화 처리 HEARTBEAT 수신 구현 */
        if (recv_len > 0) {
            mavlink_message_t recv_msg;
            mavlink_status_t recv_status;
            for (int i = 0; i < recv_len; i++) {
                if (mavlink_parse_char(MAVLINK_COMM_0, recv_buf[i], &recv_msg, &recv_status)) {
                    if (recv_msg.msgid == MAVLINK_MSG_ID_V2_EXTENSION) {
                        mavlink_v2_extension_t ext;
                        mavlink_msg_v2_extension_decode(&recv_msg, &ext);

                        if (ext.message_type == MSG_TYPE_ENCRYPTED_HEARTBEAT) {
                            uint8_t dec_iv[16];
                            uint8_t dec_cipher[16];
                            memcpy(dec_iv, &ext.payload[1], 16);
                            memcpy(dec_cipher, &ext.payload[17], 16);

                            uint8_t dec_plain[32];
                            int dec_plain_len = 0;
                            if (mavlink_aes_decrypt(dec_cipher, 16, dec_plain, &dec_plain_len, dec_iv) == 0) {
                                mavlink_heartbeat_t hb_recv;
                                memcpy(&hb_recv, dec_plain, sizeof(hb_recv));
                                printf("[CLIENT] 서버 암호화 HEARTBEAT 수신 ! (sysid=%d, type=%d)\n", recv_msg.sysid, hb_recv.type);
                            }
                            else {
                                printf("[CLIENT] 서버 응답 복호화 실패 ! \n");
                            }
                        }
                    }
                }
            }
        }

        // 1Hz
        sleep(1);
    }
    close(sock);
    return 0;
}