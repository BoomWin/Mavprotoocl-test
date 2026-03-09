#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "./c_library_v2-master/common/mavlink.h"

#define LISTEN_PORT 14550

int main(void) {
    // 1) UDP 소켓 생성
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("소켓 생성 실패...\n");
        return 1;
    }

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

    printf("[SERVER] GCS (sysid=255) 포트 %d에서 대기 중 ...\n", LISTEN_PORT);

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
            if (mavlink_parse_char(MAVLINK_COMM_0, recv_buf[i], &msg, &status)) {

                printf("[SERVER] 메시지 수신 : msgid=%d, sysid=%d, compid=%d (from %s:%d)\n",
                        msg.msgid, msg.sysid, msg.compid, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                
                // 5) HEARTBEAT 처리
                if (msg.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
                    mavlink_heartbeat_t hb;
                    mavlink_msg_heartbeat_decode(&msg, &hb);
                    printf("[SERVER] HEARTBEAT: type=%d, autopilot=%d, status=%d\n", 
                            hb.type, hb.autopilot, hb.system_status);
                    
                    // 6) 응답 : GCS도 HEARTBEAT를 클라이언트에게 보냄
                    mavlink_message_t reply;
                    uint8_t send_buf[MAVLINK_MAX_PACKET_LEN];

                    mavlink_msg_heartbeat_pack(
                        255,    // system_id (GCS)
                        190,    // component_id (GCS)
                        &reply, 
                        MAV_TYPE_GCS,
                        MAV_AUTOPILOT_INVALID,
                        0,
                        0,
                        MAV_STATE_ACTIVE
                    );

                    uint16_t len = mavlink_msg_to_send_buffer(send_buf, &reply);
                    sendto(sock, send_buf, len, 0, (struct sockaddr*)&client_addr, client_len);
                    printf("[SERVER] HEARTBEAT 응답 전송 (seq=%d)\n", reply.seq);
                }
            }
        }
    }
    close(sock);
    return 0;
}