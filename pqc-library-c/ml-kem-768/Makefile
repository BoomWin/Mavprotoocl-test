CC = gcc
CFLAGS = -O3 -Wall -Wextra -I. -I../../../common

SOURCES = cbd.c indcpa.c kem.c ntt.c poly.c polyvec.c \
		reduce.c symmetric-shake.c verify.c
COMMON = ../../../common/fips202.c ../../../common/randombytes.c

test_kem: $(SOURCES) $(COMMON) test_kem.c
	$(CC) $(CFLAGS) -o $@ $^

pqc_cli: $(SOURCES) $(COMMON) pqc_cli.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f test_kem