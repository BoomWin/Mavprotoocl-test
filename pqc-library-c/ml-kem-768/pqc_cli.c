/**
ML-KEM-512 CLI Wrapper Node.js 적용하기 위함

사용 명령어
    ./pqc_cli keygen
    ./pqc_cli encap <pk_hex>
    ./pqc_cli decap <sk_hex> <ct_hex> // SMAUG-T 같은 경우는 다름

* 
*/

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "api.h"

#include "kem.h" // derand 함수 자체가 kem.h 에 있어서 추가필요함.

// 상수 정의
#define SK_BYTES PQCLEAN_MLKEM768_CLEAN_CRYPTO_SECRETKEYBYTES // 2400
#define PK_BYTES PQCLEAN_MLKEM768_CLEAN_CRYPTO_PUBLICKEYBYTES // 1184
#define CT_BYTES PQCLEAN_MLKEM768_CLEAN_CRYPTO_CIPHERTEXTBYTES // 1088
#define SS_BYTES PQCLEAN_MLKEM768_CLEAN_CRYPTO_BYTES           // 32

#define COINS_BYTES 32

// Hex ===> Bytes 변환 
int hex_to_bytes(const char *hex, uint8_t *bytes, size_t bytes_len) {
    size_t hex_len = strlen(hex);
    // 길이 체크로직임 (혹시나 손실 여부 파악)
    if (hex_len != bytes_len * 2) {
        return -1;
    }

    // 16진수 문자열 ==> 바이트로 추출하는 영역
    for (size_t i = 0; i < bytes_len; i++) {
        unsigned int byte;
        if (sscanf(hex + 2 * i, "%02x", &byte) != 1) {
            return -1;
        }
        bytes[i] = (uint8_t)byte;
    }
    return 0; 
}

// Bytes ===> Hex 변환
char* bytes_to_hex(const uint8_t *bytes, size_t len) {
    char *hex = malloc(len * 2 + 1);
    if (!hex) {
        return NULL;
    }
    for (size_t i = 0; i < len; i++) {
        sprintf(hex + 2 * i, "%02x", bytes[i]);
    }
    hex[len * 2] = '\0';
    return hex;
}

// 에러 출력 (JSON 형식)
void print_error(const char *msg) {
    printf("{\"error\":\"%s\"}\n", msg);
}

/*=====================================================================
* KEYGEN : 키 쌍 생성
*======================================================================*/

// 이 함수는 랜덤한 값을 자동 생성해서 수행하는 KEYGEN 임
int do_keygen(void) {
    uint8_t pk[PK_BYTES];
    uint8_t sk[SK_BYTES];

    // 키 생성 호출
    // 0일때가 올바르게 생성임 확인함 -> 
    // return 1이면 잘못된거임 .!
    // 이 함수가 RANDOM_KEYPAIR 함수임. 
    if (PQCLEAN_MLKEM768_CLEAN_crypto_kem_keypair(pk, sk) != 0) {
        print_error("keygen failed\n");
        return 1;
    }

    // Bytes -> Hex 변환
    char *pk_hex = bytes_to_hex(pk, PK_BYTES);
    char *sk_hex = bytes_to_hex(sk, SK_BYTES);

    if (!pk_hex || !sk_hex) {
        print_error("메모리 할당 실패!\n");
        free(pk_hex);
        free(sk_hex);
        return 1;
    }

    // JSON 형태로 출력
    // pk(key) : %s(value)값이 될거고 sk도 동일하게.
    printf("{\"pk\":\"%s\",\"sk\":\"%s\"}\n", pk_hex, sk_hex);

    free(pk_hex);
    free(sk_hex);
    return 0;
}

// 추후에 내가 원하는 coin(랜덤)을 넣어서 key 생성하게 구현할 수 있음!
// do_keygen_derand() 함수 구현 필요.


/*=====================================================================
* [INPUT]KEYGEN : 키 쌍 생성 (coin 값을 input으로 받아서)
*======================================================================*/
int do_keygen_derand(const char *coins_hex) {
    uint8_t pk[PK_BYTES];
    uint8_t sk[SK_BYTES];
    // 32가 맞음. 
    uint8_t coins[COINS_BYTES];

    // hex => bytes
    // 64에서 => 32로 가야댐
    if (hex_to_bytes(coins_hex, coins, COINS_BYTES) != 0) {
        print_error("유효하지 않은 coins HEX (64 바이트)");
        return 1;
    }

    // derand 키 생서 호출
    if (PQCLEAN_MLKEM768_CLEAN_crypto_kem_keypair_derand(pk, sk, coins) != 0) {
        print_error("수동 입력 Key Gen 실패 .. \n");
        return 1;
    }

    char *pk_hex = bytes_to_hex(pk, PK_BYTES);
    char *sk_hex = bytes_to_hex(sk, SK_BYTES);

    if (!pk_hex || !sk_hex) {
        print_error("동적 메모리 할당 실패\n");
        free(pk_hex);
        free(sk_hex);
        return 1;   
    }

    printf("{\"pk\":\"%s\", \"sk\":\"%s\"}\n", pk_hex, sk_hex);
    free(pk_hex);
    free(sk_hex);

    return 0;
}


/*=====================================================================
* Encapsulation : 공개키 사용하여 CT 생성 과 동시에 Shared Secret 생성
*======================================================================*/

int do_encap(const char *pk_hex) {
    uint8_t pk[PK_BYTES];
    uint8_t ct[SK_BYTES];
    uint8_t ss[SS_BYTES];   // Shared Secret 값임.

    // Hex -> Bytes 변환
    // 웹에서 받은 HEX 값을 연산을 수행하기 위해 BYTES 값으로 받은 내용임. 
    if (hex_to_bytes(pk_hex, pk, PK_BYTES) != 0) {
        print_error("유효하지 않은 공개키 HEX 값임\n");
        return 1;
    }

    // 캡슐화 호출
    // 동일하게 0값을 받았을 때가 올바르게 수행되었을때임.  ==> SUCCESS == 0
    if (PQCLEAN_MLKEM768_CLEAN_crypto_kem_enc(ct, ss, pk) != 0) {
        print_error("Encapsulation 실패 \n");
        return 1;
    }

    // Bytes -> Hex 변환
    // 웹에 전달해주기 위해서 연산 후에 Hex 값으로 변환
    char *ct_hex = bytes_to_hex(ct, CT_BYTES);
    char *ss_hex = bytes_to_hex(ss, SS_BYTES);

    // 내부에서 동적할당 수행해줌. 
    if (!ct_hex || !ss_hex) {
        print_error("메모리 할당 실패\n");
        free(ct_hex);
        free(ss_hex);
        return 1;
    }

    // JSON 출력
    printf("{\"ct\":\"%s\", \"ss\":\"%s\"}\n", ct_hex, ss_hex);

    free(ct_hex);
    free(ss_hex);
    return 0;
}

/*=====================================================================
* Encapsulation : 공개키 사용하여 CT 생성 과 동시에 Shared Secret 생성
*======================================================================*/
int do_encap_derand(const char *pk_hex, const char *coins_hex) {
    uint8_t pk[PK_BYTES];
    uint8_t ct[CT_BYTES];
    uint8_t ss[SS_BYTES];
    uint8_t coins[COINS_BYTES];

    if (hex_to_bytes(pk_hex, pk, PK_BYTES) != 0) {
        print_error("유효하지 않는 공개키 HEX 값...");
        return 1;
    }

    if (hex_to_bytes(coins_hex, coins, COINS_BYTES) != 0) {
        print_error("유효하지 않은 COINS HEX 값 (64자 필요)");
        return 1;
    }

    if (PQCLEAN_MLKEM768_CLEAN_crypto_kem_enc_derand(ct, ss, pk, coins) != 0) {
        print_error("Encapsulation 실패 ... ");
        return 1;
    }

    char *ct_hex = bytes_to_hex(ct, CT_BYTES);
    char *ss_hex = bytes_to_hex((ss), SS_BYTES);

    if (!ct_hex || !ss_hex) {
        print_error("동적 메모리 할당 실패 ... ");
        free(ct_hex);
        free(ss_hex);
        return 1;
    }

    printf("{\"ct\":\"%s\",\"ss\":\"%s\"}\n", ct_hex, ss_hex);
    free(ct_hex);
    free(ss_hex);
    return 0;
}

/*=====================================================================
* Decapsulation : 개인키 사용하여 CT 디캡슐레이션 수행하여 ss 복원
*======================================================================*/

int do_decap(const char *sk_hex, const char *ct_hex) {
    uint8_t sk[SK_BYTES];
    uint8_t ct[CT_BYTES];
    uint8_t ss[SS_BYTES];

    // Hex -> Bytes 변환
    if (hex_to_bytes(sk_hex, sk, SK_BYTES) != 0) {
        print_error("유효하지 않는 SecretKey");
        return 1;
    }

    if (hex_to_bytes(ct_hex, ct, CT_BYTES) != 0) {
        print_error("유효하지 않는 암호문");
        return 1;
    }

    if (PQCLEAN_MLKEM768_CLEAN_crypto_kem_dec(ss, ct, sk) != 0) {
        print_error("Decapsulation Failed");
        return 1;
    }

    // Bytes -> hex 다시 변환 (변환 이유는 모두 동일함. 웹에서 처리하기 위함)
    char *ss_hex = bytes_to_hex(ss, SS_BYTES);

    if (!ss_hex) {
        print_error("메모리 할당 실패\n");
        return -1;
    }

    // JSON 출력
    printf("{\"ss\":\"%s\"}\n", ss_hex);

    free(ss_hex);
    return 0;
}

/*=====================================================================
* MAIN 명령어 파싱 및 실행
*======================================================================*/

int main(int argc, char *argv[]) {
    // 뒤에 나올 부분은 보안레벨을 의미할
    if (argc < 2) {
        print_error("사용 방법 : pqc_cli <keygen 혹은 encap 혹은 decap> [args ...]");
        return -1;
    }

    const char *cmd = argv[1];

    // keygen 명령
    if (strcmp(cmd, "keygen") == 0) {
        return do_keygen();
    }

    // keygen 수동 입력 명령
    if (strcmp(cmd, "keygen_derand") == 0) {
        if (argc < 3) {
            print_error("사용 방법 : pqc_cli keygen_derand <coins_hex 64자>");
            return 1;
        }
        return do_keygen_derand(argv[2]);
    }


    // encapsulation 명령
    if (strcmp(cmd, "encap") == 0) {
        if (argc < 3) {
            // 이 부분을 이제 web 영역에서 맞춰 줘야지
            print_error("사용 방법 : pqc_cli encap <pk_hex값>");
        }
        return do_encap(argv[2]);
    }
    
    // encap 수동 입력 명령
    if (strcmp(cmd, "encap_derand") == 0) {
        if (argc < 4) {
            print_error("사용 방법 : pqc_cli encap_derand <pk_hex> <coins_hex 64자>");\
            return 1;
        }
        return do_encap_derand(argv[2], argv[3]);
    }

    // decapsulation 명령
    if (strcmp(cmd, "decap") == 0) {
        if (argc < 4) {
            print_error("사용 방법 : pqc_cli decap <sk_hex값> <ct_hex값>");
            return 1;
        }
        return do_decap(argv[2], argv[3]);
    }

    print_error("[주의] 유효하지 않은 명령어 !!!\n");
    return 1;
}