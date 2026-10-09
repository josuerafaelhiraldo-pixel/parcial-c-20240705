/* Pruebas en PC de la consola (console.c) usando el simulador de HAL. */
#include "console.h"
#include "hal_sim.h"
#include "usart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALLO linea %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void)
{
    uint8_t b; ConsoleStats st;

    Console_Init();
    CHECK(sim_rx_arm_calls == 1);                       /* RX armada al iniciar */

    /* --- TX: un mensaje se envia completo --- */
    CHECK(Console_Write("hola"));
    CHECK(sim_tx_calls == 1);
    /* mientras hay una transmision en curso, un segundo mensaje espera en la cola */
    CHECK(Console_Write(" mundo"));
    CHECK(sim_tx_calls == 1);
    sim_tx_drain();
    CHECK(sim_tx_out_len == 10 && memcmp(sim_tx_out, "hola mundo", 10) == 0);
    sim_tx_out_len = 0;

    /* --- TX: muchos mensajes de tamanos al azar, completando de forma intercalada,
     *     fuerza el paso por el final del buffer circular --- */
    {
        char expected[200000]; size_t elen = 0; srand(7);
        for (int i = 0; i < 20000; i++) {
            char msg[64]; int n = 1 + rand() % 60;
            for (int k = 0; k < n; k++) msg[k] = (char)('a' + rand() % 26);
            msg[n] = 0;
            if (Console_Write(msg)) { memcpy(&expected[elen], msg, (size_t)n); elen += (size_t)n; }
            Console_Service();
            if (rand() % 3 == 0) sim_tx_complete();     /* la "ISR" termina a destiempo */
            if (elen > 150000) break;
        }
        sim_tx_drain();
        CHECK(sim_tx_out_len == elen);
        CHECK(memcmp(sim_tx_out, expected, elen) == 0); /* mismo flujo, mismo orden, sin corrupcion */
        sim_tx_out_len = 0;
    }

    /* --- TX: cola llena -> el mensaje se descarta ENTERO y se cuenta --- */
    {
        char big[300]; memset(big, 'x', sizeof big - 1); big[sizeof big - 1] = 0;
        Console_GetStats(&st); uint32_t d0 = st.txDropped;
        CHECK(Console_Write(big));                      /* 299 bytes caben (cola de 512) */
        CHECK(!Console_Write(big));                     /* otros 299 ya no caben */
        Console_GetStats(&st);
        CHECK(st.txDropped == d0 + 1);
        sim_tx_drain();
        CHECK(sim_tx_out_len == 299);                   /* solo salio el primero, completo */
        sim_tx_out_len = 0;
    }

    /* --- RX: bytes en orden --- */
    for (int i = 0; i < 100; i++) sim_rx_byte((uint8_t)i);
    for (int i = 0; i < 100; i++) { CHECK(Console_ReadByte(&b)); CHECK(b == i); }
    CHECK(!Console_ReadByte(&b));

    /* --- RX: desbordamiento del buffer (sin que main lea) --- */
    Console_GetStats(&st); uint32_t ov0 = st.rxRingOverflow, rx0 = st.rxBytes;
    for (int i = 0; i < 300; i++) sim_rx_byte((uint8_t)i);
    Console_GetStats(&st);
    CHECK(st.rxBytes - rx0 == 300);
    CHECK(st.rxRingOverflow - ov0 == 300 - 255);        /* capacidad util 255 */
    int got = 0; while (Console_ReadByte(&b)) { CHECK(b == (uint8_t)got); got++; }
    CHECK(got == 255);                                  /* se conservan los PRIMEROS 255, en orden */
    /* y se recupera sin intervencion */
    sim_rx_byte('Z'); CHECK(Console_ReadByte(&b) && b == 'Z');

    /* --- errores de hardware --- */
    int arms = sim_rx_arm_calls;
    sim_rx_error(HAL_UART_ERROR_ORE);                   /* ORE detiene la recepcion... */
    Console_GetStats(&st); CHECK(st.uartOverrun == 1);
    CHECK(sim_rx_arm_calls == arms + 1);                /* ...y el callback la rearma */
    sim_rx_byte('Q'); CHECK(Console_ReadByte(&b) && b == 'Q');   /* sigue recibiendo */
    sim_rx_error(HAL_UART_ERROR_FE | HAL_UART_ERROR_NE | HAL_UART_ERROR_PE);
    Console_GetStats(&st);
    CHECK(st.uartFraming == 1 && st.uartNoise == 1 && st.uartParity == 1);

    /* --- recuperacion desde Console_Service si la RX quedo detenida --- */
    huart2.RxState = HAL_UART_STATE_READY;              /* simula RX parada */
    arms = sim_rx_arm_calls;
    Console_Service();
    CHECK(sim_rx_arm_calls == arms + 1);
    sim_rx_byte('R'); CHECK(Console_ReadByte(&b) && b == 'R');

    /* --- Console_Printf --- */
    sim_tx_out_len = 0;
    CHECK(Console_Printf("v=%d,%s", 42, "ok"));
    sim_tx_drain();
    CHECK(sim_tx_out_len == 7 && memcmp(sim_tx_out, "v=42,ok", 7) == 0);
    sim_tx_out_len = 0;
    {   /* mensaje mas largo que el maximo: se trunca sin desbordar nada */
        char longfmt[400]; memset(longfmt, 'y', sizeof longfmt - 1); longfmt[sizeof longfmt - 1] = 0;
        CHECK(Console_Printf("%s", longfmt));
        sim_tx_drain();
        CHECK(sim_tx_out_len == 127);                   /* APP_PRINTF_MAX - 1 */
    }

    printf(fails ? "CONSOLE: %d FALLOS\n" : "CONSOLE: todas las pruebas OK\n", fails);
    return fails != 0;
}
