/*
* test_bench_kem.c - KEM 알고리즘 성능 측정용
*
* [구현 특성]
* - CLEAN (Non-AVX2) 구현 - 하드웨어 SIMD 가속 없이 순수 C 코드로 동작
* - PQClean 기준: 함수명에 _CLEAN_ 포함 (AVX2라면 _AVX2_ 로 표기됨)
* - 벤치마크 결과는 AVX2 최적화 버전 대비 보통 2~5배 느림
*
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "api.h"

/* ML-KEM (PQClean 스타일) */
#define BENCH_PK_BYTES  PQCLEAN_MLKEM768_CLEAN_CRYPTO_PUBLICKEYBYTES
#define BENCH_SK_BYTES  PQCLEAN_MLKEM768_CLEAN_CRYPTO_SECRETKEYBYTES
#define BENCH_CT_BYTES  PQCLEAN_MLKEM768_CLEAN_CRYPTO_CIPHERTEXTBYTES
#define BENCH_SS_BYTES  PQCLEAN_MLKEM768_CLEAN_CRYPTO_BYTES
#define BENCH_ALGNAME   PQCLEAN_MLKEM768_CLEAN_CRYPTO_ALGNAME

/* ML-KEM (PQClean 스타일) */
#define BENCH_KEYPAIR(pk, sk)       PQCLEAN_MLKEM768_CLEAN_crypto_kem_keypair(pk, sk)
#define BENCH_ENC(ct, ss, pk)       PQCLEAN_MLKEM768_CLEAN_crypto_kem_enc(ct, ss, pk)
#define BENCH_DEC(ss, ct, sk)       PQCLEAN_MLKEM768_CLEAN_crypto_kem_dec(ss, ct, sk)

static double elapsed_ms(struct timespec s, struct timespec e) {
    return (e.tv_sec - s.tv_sec) * 1000.0
         + (e.tv_nsec - s.tv_nsec) / 1e6;
}

int main(void) {
    int ITERATIONS = 1000; /* 빠른 알고리즘은 크게, SPHINCS+ 같이 느리면 작게 */

    uint8_t pk[BENCH_PK_BYTES];
    uint8_t sk[BENCH_SK_BYTES];
    uint8_t ct[BENCH_CT_BYTES];
    uint8_t ss_enc[BENCH_SS_BYTES];
    uint8_t ss_dec[BENCH_SS_BYTES];

    struct timespec t0, t1;
    int i;

    printf("=============================================================\n");
    printf(" Algorithm : %s\n", BENCH_ALGNAME);
    printf(" Implementation : CLEAN (Non-AVX2, pure C)\n");
    printf(" Iterations : %d\n", ITERATIONS);
    printf("=============================================================\n\n");

    /* ---- 1. KeyGen ---- */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (i = 0; i < ITERATIONS; i++) {
        if (BENCH_KEYPAIR(pk, sk) != 0) {
            fprintf(stderr, "KeyGen failed at iteration %d\n", i);
            return 1;
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double keygen_ms = elapsed_ms(t0, t1) / ITERATIONS;

    /* ---- 2. Encap ---- */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (i = 0; i < ITERATIONS; i++) {
        if (BENCH_ENC(ct, ss_enc, pk) != 0) {
            fprintf(stderr, "Encap failed at iteration %d\n", i);
            return 1;
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double encap_ms = elapsed_ms(t0, t1) / ITERATIONS;

    /* ---- 3. Decap ---- */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (i = 0; i < ITERATIONS; i++) {
        if (BENCH_DEC(ss_dec, ct, sk) != 0) {
            fprintf(stderr, "Decap failed at iteration %d\n", i);
            return 1;
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double decap_ms = elapsed_ms(t0, t1) / ITERATIONS;

    /* ---- 정확성 확인 ---- */
    int ss_ok = (memcmp(ss_enc, ss_dec, BENCH_SS_BYTES) == 0);

    /* ---- 결과 출력 ---- */
    printf("[Size]\n");
    printf("  pk_bytes : %d\n", BENCH_PK_BYTES);
    printf("  sk_bytes : %d\n", BENCH_SK_BYTES);
    printf("  ct_bytes : %d\n", BENCH_CT_BYTES);
    printf("  ss_bytes : %d\n", BENCH_SS_BYTES);
    printf("\n");
    printf("[Performance (avg over %d runs)]\n", ITERATIONS);
    printf("  KeyGen : %.4f ms\n", keygen_ms);
    printf("  Encap  : %.4f ms\n", encap_ms);
    printf("  Decap  : %.4f ms\n", decap_ms);
    printf("\n");
    printf("[Correctness]\n");
    printf("  Shared secret match : %s\n", ss_ok ? "OK" : "FAIL");
    printf("=============================================================\n");

    return ss_ok ? 0 : 1;
}