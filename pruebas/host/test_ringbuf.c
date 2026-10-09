/* Pruebas en PC del buffer circular (ringbuf.c). */
#include "ringbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALLO linea %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void test_init(void)
{
    uint8_t st[64]; RingBuf rb;
    CHECK(!RingBuf_Init(&rb, st, 0));
    CHECK(!RingBuf_Init(&rb, st, 3));
    CHECK(!RingBuf_Init(&rb, st, 100));
    CHECK(RingBuf_Init(&rb, st, 64));
    CHECK(RingBuf_Count(&rb) == 0 && RingBuf_Free(&rb) == 63);
}

static void test_full_empty(void)
{
    uint8_t st[8]; RingBuf rb; uint8_t b = 0;
    RingBuf_Init(&rb, st, 8);
    CHECK(!RingBuf_Get(&rb, &b));                       /* vacio */
    for (int i = 0; i < 7; i++) CHECK(RingBuf_Put(&rb, (uint8_t)(10 + i)));
    CHECK(!RingBuf_Put(&rb, 99));                       /* lleno: capacidad = size-1 */
    CHECK(RingBuf_Count(&rb) == 7 && RingBuf_Free(&rb) == 0);
    for (int i = 0; i < 7; i++) { CHECK(RingBuf_Get(&rb, &b)); CHECK(b == 10 + i); }
    CHECK(!RingBuf_Get(&rb, &b));
}

static void test_capacity_rx(void)
{
    uint8_t st[256]; RingBuf rb; int ok = 0;
    RingBuf_Init(&rb, st, 256);
    for (int i = 0; i < 300; i++) if (RingBuf_Put(&rb, (uint8_t)i)) ok++;
    CHECK(ok == 255);                                   /* >= 128 exigidos por el reto */
}

static void test_random_vs_model(void)
{
    uint8_t st[16]; RingBuf rb; uint8_t model[4096]; size_t mh = 0, mt = 0;
    RingBuf_Init(&rb, st, 16);
    srand(1234);
    for (int it = 0; it < 200000; it++) {
        if (rand() % 2) {
            uint8_t v = (uint8_t)rand();
            bool full = (mh - mt) >= 15;
            bool ok = RingBuf_Put(&rb, v);
            CHECK(ok == !full);
            if (ok) model[mh++ % sizeof model] = v;
        } else {
            uint8_t v = 0; bool empty = (mh == mt);
            bool ok = RingBuf_Get(&rb, &v);
            CHECK(ok == !empty);
            if (ok) CHECK(v == model[mt++ % sizeof model]);
        }
        CHECK(RingBuf_Count(&rb) == (uint16_t)(mh - mt));
    }
}

static void test_contiguous(void)
{
    uint8_t st[8]; RingBuf rb; uint8_t b;
    RingBuf_Init(&rb, st, 8);
    for (int i = 0; i < 6; i++) RingBuf_Put(&rb, (uint8_t)i);
    for (int i = 0; i < 5; i++) RingBuf_Get(&rb, &b);            /* tail = 5 */
    for (int i = 0; i < 5; i++) RingBuf_Put(&rb, (uint8_t)(20 + i)); /* da la vuelta */
    CHECK(RingBuf_Count(&rb) == 6);
    CHECK(RingBuf_Contiguous(&rb) == 3);                          /* hasta el final del arreglo */
    CHECK(*RingBuf_TailPtr(&rb) == 5);
    RingBuf_Skip(&rb, 3);
    CHECK(RingBuf_Contiguous(&rb) == 3);                          /* resto, ya sin dar la vuelta */
    CHECK(*RingBuf_TailPtr(&rb) == 22);   /* 5, 20 y 21 ya salieron */
    RingBuf_Skip(&rb, 3);
    CHECK(RingBuf_Count(&rb) == 0 && RingBuf_Contiguous(&rb) == 0);
}

int main(void)
{
    test_init(); test_full_empty(); test_capacity_rx(); test_random_vs_model(); test_contiguous();
    printf(fails ? "RINGBUF: %d FALLOS\n" : "RINGBUF: todas las pruebas OK\n", fails);
    return fails != 0;
}
