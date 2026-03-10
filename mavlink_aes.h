#ifndef MAVLINK_AES_H
#define MAVLINK_AES_H

#include <cstddef>
#include <cstdint>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/types.h>
#include <string.h>

// 서버 / 클라이언트 공유 키 (16byte)
static const uint8_t AES_KEY[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,                                                                                                                                                                                                                        
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F  
};

// V2 EXTENSION의 message_type 정의
#define MSG_TYPE_ENCRYPTED_HEARTBEAT    0x0001

// V2 EXTENSION payload 설계
// [0] : 원본 메시지 길이
// [1~16] : IV (16 바이트)
// [17~32] : 암호문 (16바이트) - HEARTBEAT 9바이트 + PKCS#7 패딩
// 총 33바이트

/* AES-128-CBC 암호화 */
static inline int mavlink_aes_encrypt(const uint8_t *plaintext, int plain_len,
                                        uint8_t *ciphertext, int *cipher_len,
                                        uint8_t *iv)
{

    // 랜덤 IV 생성
    if (RAND_bytes(iv, 16) != 1)    return -1;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)   return -1;

    int len = 0;
    *cipher_len = 0;

    // 어떤 암호를 사용할지
    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, AES_KEY, iv) != 1)
        goto err;
    // 암호화 수행
    if (EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plain_len) != 1)
        goto err;
    *cipher_len = len;
    // 패딩 처리 cipehr_len을 통해 작업한 구간 뒤에 이어서 패딩 처리 API
    if (EVP_EncryptFinal_ex(ctx, ciphertext + len, &len) != 1)
        goto err;
    *cipher_len += len;

    EVP_CIPHER_CTX_free(ctx);
    return 0;

    err:
        EVP_CIPHER_CTX_free(ctx);
        return -1;
}

/* AES-128-CBC 복호화 */
static inline int mavlink_aes_decrypt(const uint8_t *ciphertext, int cipher_len,
                                        uint8_t *plaintext, int *plain_len,
                                        const uint8_t *iv)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)   return -1;

    int len = 0;
    *plain_len = 0;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, AES_KEY, iv) != 1)
        goto err;
    if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, cipher_len) != 1)
        goto err;
    *plain_len = len;
    if (EVP_DecryptFinal_ex(ctx, plaintext + len, &len) != 1)
        goto err;
    *plain_len += len;

    EVP_CIPHER_CTX_free(ctx);
    return 0;

    err:
        EVP_CIPHER_CTX_free(ctx);
        return -1;
}

#endif