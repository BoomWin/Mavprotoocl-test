#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "./c_library_v2-master/common/mavlink.h"


#define SERVER_IP   "127.0.0.1" // 자신 IP
#define SERVER_PORT 14550   // GCS에서 해당 포트를 표준처럼 디폴트로 사용한다고함

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

    printf("[CLIENT] Vehicle (sysid = 1) 시작, 서버 %s:%d로 HEARTBEAT 전송\n", SERVER_IP, SERVER_PORT);

    // 3) 메인 루프 : 1초마다 HEARTBEAT 송신 + 서버 응답 수신
    while (1) {
        // 송신 부분
        mavlink_message_t msg;
        uint8_t buf[MAVLINK_MAX_PACKET_LEN];

        // HEARTBEAT 패킹 : sysid=1, compid=1 (Vehicle)
        mavlink_msg_heartbeat_pack(
            1,                      // system_id (Vehicle)
            1,                      // component_id
            &msg,
            MAV_TYPE_QUADROTOR,      // type: 쿼드 콥터터
            MAV_AUTOPILOT_GENERIC,   // autopilot: 일반 자동조종 장치
            MAV_MODE_FLAG_CUSTOM_MODE_ENABLED, // base_mode: 사용자 모드 활성화
            0,                        // custom_mode: 사용자 모드 0
            MAV_STATE_ACTIVE          // system_status: 활성 상태
        );

        // 직렬화
        uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);

        // UDP 전송
        sendto(sock, buf, len, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
        printf("[CLIENT] HEARTBEAT 전송 (seq=%d, %d bytes)\n", msg.seq, len);

        // 수신 부분
        struct timeval tv = {0, 100000}; // 100ms timeout
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        uint8_t recv_buf[MAVLINK_MAX_PACKET_LEN];
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        ssize_t recv_len = recvfrom(sock, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&from_addr, &from_len);


        if (recv_len > 0) {
            // 바이트 단위 파싱
            mavlink_message_t recv_msg;
            mavlink_status_t status;
            for (int i = 0; i < recv_len; i++) {
                if (mavlink_parse_char(MAVLINK_COMM_0, recv_buf[i], &recv_msg, &status)) {
                    if (recv_msg.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
                        mavlink_heartbeat_t hb;
                        mavlink_msg_heartbeat_decode(&recv_msg, &hb);
                        printf("[CLIENT] 서버 HEART BEAT 수신 ... (sysid=%d, type=%d)\n", 
                                recv_msg.sysid, hb.type);
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