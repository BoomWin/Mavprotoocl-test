#include "c_library_v2-master/minimal/mavlink_msg_heartbeat.h"
#include "mavlink_aes.h"

#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "./c_library_v2-master/common/mavlink.h"

#define LISTEN_PORT 14550

// 서명 없는 패킷 처리 어떻게 할지
// 서명 없는 패킷 거부 (1 = 허용, 0 = 거부)
static int accept_unsigned(const mavlink_status_t *status, uint32_t msgid) {
    // 모든 unsigned 패킷은 거부.
    return 0;
}

int main(void) {
    // 1) UDP 소켓 생성
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("소켓 생성 실패...\n");
        return 1;
    }
    int sign_error = 0;
    // 2) 바인딩
    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(LISTEN_PORT);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&my_addr, sizeof(my_addr)) < 0) {
        perror("바인딩 실패...\n");
        close(sock);
        return 1;
    }

    // 검증을 위한 필요 변수 선언
    mavlink_signing_t signing;
    mavlink_signing_streams_t signing_streams;

    // client와 동일한 키로 구성. (현재는 임시)
    uint8_t secret_key[32] = {                                                                                                                    
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,                                                                                           
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,                                                                                           
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,                                                                                           
        0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20                                                                                            
    }; 

    // // 예시로 조금 키를 다르게 줘보기 테스트 
    // uint8_t secret_key[32] = {                                                                                                                    
    //     0x03, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,                                                                                           
    //     0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,                                                                                           
    //     0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,                                                                                           
    //     0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20                                                                                            
    // }; 

    // 클라이언트와 동일한 키를 맞춰줘야함 그래서 어쨌든 이러한 서명을 생성하기 위해서도 키가 필요하다는뜻임 (키교환 구현 , then 개체 인증도 구현 필요)
    memset(&signing, 0, sizeof(signing));
    memset(&signing_streams, 0, sizeof(signing_streams)); 
   
    memcpy(signing.secret_key, secret_key, 32);
    // signing 컨텍스트를 채널에 등록
    signing.accept_unsigned_callback = accept_unsigned;

    printf("[SERVER] GCS (sysid=255) 포트 %d에서 대기 중 ...\n", LISTEN_PORT);

    mavlink_status_t *ch_status = mavlink_get_channel_status(MAVLINK_COMM_0);
    ch_status->signing = &signing;
    ch_status->signing_streams = &signing_streams;

    // 3) 수신 루프
    while (1) {
        uint8_t recv_buf[MAVLINK_MAX_PACKET_LEN];
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // UDP 수신 (블로킹)
        ssize_t recv_len = recvfrom(sock, recv_buf, sizeof(recv_buf), 0, (struct sockaddr*)&client_addr, &client_len);
        if (recv_len <= 0)  continue;
        
        // 4) MAVLink 파싱 : 바이트 단위 루프
        mavlink_message_t msg;
        mavlink_status_t status;
  
        for (int i = 0; i < recv_len; i++) {
            // 1= 정상처리, 0=무언가잘못됨
            if (mavlink_parse_char(MAVLINK_COMM_0, recv_buf[i], &msg, &status)) {
            
                printf("[SERVER] 메시지 수신 : msgid=%d, sysid=%d, compid=%d (from %s:%d)\n",
                        msg.msgid, msg.sysid, msg.compid, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                

                /* ================ 기존 HEARTBEAT 처리 ==================*/
                // 5) HEARTBEAT 처리
                // if (msg.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
                //     mavlink_heartbeat_t hb;
                //     mavlink_msg_heartbeat_decode(&msg, &hb);
                //     printf("[SERVER] HEARTBEAT: type=%d, autopilot=%d, status=%d\n", 
                //             hb.type, hb.autopilot, hb.system_status);
                    
                //     // 6) 응답 : GCS도 HEARTBEAT를 클라이언트에게 보냄
                //     mavlink_message_t reply;
                //     uint8_t send_buf[MAVLINK_MAX_PACKET_LEN];

                //     mavlink_msg_heartbeat_pack(
                //         255,    // system_id (GCS)
                //         190,    // component_id (GCS)
                //         &reply, 
                //         MAV_TYPE_GCS,
                //         MAV_AUTOPILOT_INVALID,
                //         0,
                //         0,
                //         MAV_STATE_ACTIVE
                //     );

                //     uint16_t len = mavlink_msg_to_send_buffer(send_buf, &reply);
                //     sendto(sock, send_buf, len, 0, (struct sockaddr*)&client_addr, client_len);
                //     printf("[SERVER] HEARTBEAT 응답 전송 (seq=%d)\n", reply.seq);
                //     sign_error = 0;
                // }

                /* ============== V2 Extension 암호화된 HEARTBEAT 처리 ============== */
                if (msg.msgid == MAVLINK_MSG_ID_V2_EXTENSION) {
                    mavlink_v2_extension_t ext;
                    mavlink_msg_v2_extension_decode(&msg, &ext);

                    if (ext.message_type == MSG_TYPE_ENCRYPTED_HEARTBEAT) {
                        // 이걸 규칙으로 0번 페이로드에 기존 하트비트의 길이를 담아줬음.
                        uint8_t original_len = ext.payload[0];
                        uint8_t iv[16];
                        uint8_t ciphertext[16];
                        // 송신 측에서 1~16까지를 iv 값 데이터 넣어줬었음 
                        memcpy(iv, &ext.payload[1], 16);
                        // 송신 측에서 17~32까지 ct값 넣어줫음
                        memcpy(ciphertext, &ext.payload[17], 16);

                        // 복호화
                        uint8_t plaintext[32];
                        int plain_len = 0;
                        if (mavlink_aes_decrypt(ciphertext, 16, plaintext, &plain_len, iv) == 0) {
                            mavlink_heartbeat_t hb;
                            memcpy(&hb, plaintext, MAVLINK_MSG_ID_HEARTBEAT_LEN);
                            printf("[SERVER] 복호화 성공 ! type=%d, autopilot=%d, status=%d\n",
                                    hb.type, hb.autopilot, hb.system_status);
                        }
                        else {
                            printf("[SERVER] 복호화 실패! \n");
                        }

                        // 응답 (암호화 적용)
                        mavlink_heartbeat_t reply_hb;
                        memset(&reply_hb, 0, sizeof(reply_hb));
                        reply_hb.custom_mode            = 0;
                        reply_hb.type                   = MAV_TYPE_GCS;
                        reply_hb.autopilot              = MAV_AUTOPILOT_INVALID;
                        reply_hb.base_mode              = 0;
                        reply_hb.system_status          = MAV_STATE_ACTIVE;
                        reply_hb.mavlink_version        = 3;

                        uint8_t reply_iv[16];
                        uint8_t reply_cipher[32];
                        int reply_cipher_len = 0;

                        if (mavlink_aes_encrypt((uint8_t *)&reply_hb, MAVLINK_MSG_ID_HEARTBEAT_LEN, reply_cipher, &reply_cipher_len, reply_iv) == 0) {
                            uint8_t reply_ext_payload[249];
                            memset(reply_ext_payload, 0, sizeof(reply_ext_payload));
                            reply_ext_payload[0] = MAVLINK_MSG_ID_HEARTBEAT_LEN;
                            memcpy(&reply_ext_payload[1], reply_iv, 16);
                            memcpy(&reply_ext_payload[17], reply_cipher, reply_cipher_len);

                            mavlink_message_t reply;
                            uint8_t send_buf[MAVLINK_MAX_PACKET_LEN];
                            mavlink_msg_v2_extension_pack(
                                255,                // sysid = 255
                                190,                // compid = 190 (compid는 기체 내부의 '특정 부품(모듈)'을 뜻함) 다른 장착된 주변 기기를 의미하는듯.
                                &reply,             // reply
                                0,                  // target_network (0은 broadcast를 의미하고, 예를 들면 통신을  5g, 뭐 물리 이더넷, 위성 이런식으로 있으면 그거에 대한 특정 번호를 의미하는 것 같음.)
                                1,                  // target_system
                                1,                  // target_component
                                MSG_TYPE_ENCRYPTED_HEARTBEAT,
                                reply_ext_payload
                            );
                            uint16_t len = mavlink_msg_to_send_buffer(send_buf, &reply);
                            sendto(sock ,send_buf, len, 0, (struct sockaddr*)&client_addr, client_len);

                            printf("[SERVER] 암호화 HEARTBEAT 응답 전송 (seq=%d)\n", reply.seq);
                            sign_error = 0;
                        }
                    }
                }
            }
            // 정상 수행이 안될때 시그니처 오류인지에 대해서 체크하기 위해구현
            else {
                sign_error = 1;
            }
        }
        // 전체 다 돌고 만약의 else 들어오면 안되는거니까 
        if (sign_error == 1) {
            printf("[SERVER] SIGN_ERROR !!!!! \n");
        }
    }
    close(sock);
    return 0;
}