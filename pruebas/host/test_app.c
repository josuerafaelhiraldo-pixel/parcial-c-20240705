/* Pruebas en PC de la aplicacion del Hito 1 (app.c): teclas, LED, B1. */
#include "app.h"
#include "console.h"
#include "hal_sim.h"
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALLO linea %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void feed(const char *s) { while (*s) sim_rx_byte((uint8_t)*s++); }
static void run(void) { for (int i = 0; i < 20; i++) { App_Loop(); sim_tx_drain(); } }
static const char *out(void) { sim_tx_out[sim_tx_out_len] = 0; return (const char *)sim_tx_out; }
static void clear(void) { sim_tx_out_len = 0; }

int main(void)
{
    sim_set_btn(1);                                       /* reposo en alto (con pull-up) */
    App_Init(); sim_tx_drain();
    CHECK(strstr(out(), "Hito 1") != NULL);
    CHECK(strstr(out(), "nivel 1") != NULL);
    CHECK(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET);   /* LD2 apagado al inicio */
    clear();

    feed("1"); run();
    CHECK(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_SET);
    CHECK(strstr(out(), "encendido") != NULL); clear();
    feed("0"); run();
    CHECK(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET); clear();
    feed("t"); run(); CHECK(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_SET);
    feed("t"); run(); CHECK(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET); clear();

    feed("b"); run(); CHECK(strstr(out(), "nivel = 1") != NULL); clear();
    sim_set_btn(0); run();                                /* el pulsador baja: se avisa solo */
    CHECK(strstr(out(), "[B1] PC13 = 0 (bajo)") != NULL); clear();
    run(); CHECK(sim_tx_out_len == 0);                    /* sin cambios, no repite */
    sim_set_btn(1); run(); CHECK(strstr(out(), "PC13 = 1 (alto)") != NULL); clear();

    feed("x"); run(); CHECK(strcmp(out(), "x") == 0); clear();          /* eco */
    feed("\r"); run(); CHECK(strcmp(out(), "\r\n") == 0); clear();     /* CR */
    feed("x\n"); run(); CHECK(strcmp(out(), "x\r\n") == 0); clear();   /* LF solo (tras otro caracter) */
    feed("\r\n"); run(); CHECK(strcmp(out(), "\r\n") == 0); clear();   /* CRLF = un solo salto */

    feed("h"); run(); CHECK(strstr(out(), "Teclas de prueba") != NULL); clear();
    feed("s"); run(); CHECK(strstr(out(), "RX: bytes=") != NULL && strstr(out(), "TX: bytes=") != NULL);
    clear();

    /* limite de trabajo por vuelta: 100 bytes pendientes no se atienden en una sola vuelta */
    for (int i = 0; i < 100; i++) sim_rx_byte('x');
    App_Loop(); sim_tx_drain();
    CHECK(sim_tx_out_len == 32);
    run(); CHECK(sim_tx_out_len == 100);                  /* el resto, en las vueltas siguientes */

    printf(fails ? "APP: %d FALLOS\n" : "APP: todas las pruebas OK\n", fails);
    return fails != 0;
}
