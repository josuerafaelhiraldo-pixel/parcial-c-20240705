/* Pruebas en PC de la aplicacion (app.c): teclas, LED, B1 por eventos, tick. */
#include "app.h"
#include "console.h"
#include "hal_sim.h"
#include "tick.h"
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALLO linea %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void feed(const char *s) { while (*s) sim_rx_byte((uint8_t)*s++); }
static const char *out(void) { sim_tx_out[sim_tx_out_len] = 0; return (const char *)sim_tx_out; }
static void clear(void) { sim_tx_out_len = 0; }
/* n milisegundos de funcionamiento: tick de TIM6 + una vuelta de main por ms */
static void ms(uint32_t n) { while (n--) { sim_tick(1); App_Loop(); sim_tx_drain(); } }
static void run(void) { ms(20); }
static void btn(int pressed)
{
    sim_set_btn(pressed ? 0 : 1);
    HAL_GPIO_EXTI_Callback(GPIO_PIN_13);
}

int main(void)
{
    sim_set_btn(1);                                       /* reposo en alto (con pull-up) */
    App_Init(); sim_tx_drain();
    CHECK(strstr(out(), "Hito 2") != NULL);
    CHECK(strstr(out(), "nivel 1") != NULL);
    CHECK(sim_tim_start_calls == 1);                      /* App_Init arranca el tick */
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

    /* B1 ya no se avisa por sondeo: solo hay mensajes cuando se valida una pulsacion */
    sim_set_btn(0); run();
    CHECK(sim_tx_out_len == 0); clear();
    sim_set_btn(1); run(); CHECK(sim_tx_out_len == 0);

    /* pulsacion corta (con rebote) -> LD2 alterna y se informa */
    btn(1); ms(1); btn(0); ms(2); btn(1); ms(150); btn(0); ms(1); btn(1); ms(1); btn(0); ms(100);
    CHECK(strstr(out(), "[B1] corta #1") != NULL);
    CHECK(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_SET);
    CHECK(strstr(out(), "[B1] corta #2") == NULL);        /* sin duplicados por rebote */
    clear();

    /* segunda corta: LD2 vuelve a apagarse */
    btn(1); ms(100); btn(0); ms(100);
    CHECK(strstr(out(), "[B1] corta #2") != NULL);
    CHECK(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET); clear();

    /* larga: un solo mensaje, LD2 apagado, al soltar no hay corta */
    feed("1"); run(); clear();
    btn(1); ms(3000); btn(0); ms(100);
    CHECK(strstr(out(), "[B1] larga #1") != NULL);
    CHECK(strstr(out(), "[B1] larga #2") == NULL);
    CHECK(strstr(out(), "corta") == NULL);
    CHECK(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET); clear();

    /* estadisticas del boton */
    feed("s"); run();
    CHECK(strstr(out(), "RX: bytes=") != NULL && strstr(out(), "TX: bytes=") != NULL);
    CHECK(strstr(out(), "cortas=2 largas=1") != NULL);
    CHECK(strstr(out(), "estado=suelto") != NULL); clear();

    /* tecla m: TIM6 y SysTick avanzan igual */
    feed("m"); run(); clear();
    ms(1000);
    feed("m"); ms(20);
    CHECK(strstr(out(), "diferencia=0 ms") != NULL);
    clear();

    /* tecla w: tiempo inyectado a 2048 ms de UINT32_MAX; B1 sigue funcionando al cruzarlo */
    feed("w"); run();
    CHECK(strstr(out(), "UINT32_MAX en 2048 ms") != NULL); clear();
    CHECK(Tick_Ms() > 0xFFFFF000U);
    btn(1); ms(100); btn(0); ms(100);                     /* corta antes del desbordamiento */
    CHECK(strstr(out(), "[B1] corta #1") != NULL); clear();
    ms(400);
    btn(1); ms(5000); btn(0); ms(100);                    /* larga: empieza antes y termina despues del desbordamiento */
    CHECK(Tick_Ms() < 5000U);                             /* el contador dio la vuelta */
    CHECK(strstr(out(), "[B1] larga #1") != NULL && strstr(out(), "corta") == NULL);
    {
        const char *tag = "larga #1 (1500 ms), t=";
        const char *q = strstr(out(), tag);
        unsigned long tv = 999999UL;
        CHECK(q != NULL);
        if (q != NULL) { (void)sscanf(q + strlen(tag), "%lu", &tv); }
        CHECK(tv < 500UL);                                /* la larga salio DESPUES de dar la vuelta (t pequeno) */
    }
    clear();
    btn(1); ms(100); btn(0); ms(100);                     /* y despues del desbordamiento */
    CHECK(strstr(out(), "[B1] corta #2") != NULL); clear();
    feed("m"); run(); ms(1000); clear(); feed("m"); ms(20);
    CHECK(strstr(out(), "diferencia=0 ms") != NULL); clear();

    feed("x"); run(); CHECK(strcmp(out(), "x") == 0); clear();          /* eco */
    feed("\r"); run(); CHECK(strcmp(out(), "\r\n") == 0); clear();     /* CR */
    feed("x\n"); run(); CHECK(strcmp(out(), "x\r\n") == 0); clear();   /* LF solo (tras otro caracter) */
    feed("\r\n"); run(); CHECK(strcmp(out(), "\r\n") == 0); clear();   /* CRLF = un solo salto */

    feed("h"); run(); CHECK(strstr(out(), "Teclas de prueba") != NULL); clear();

    /* limite de trabajo por vuelta: 100 bytes pendientes no se atienden en una sola vuelta */
    for (int i = 0; i < 100; i++) sim_rx_byte('x');
    App_Loop(); sim_tx_drain();
    CHECK(sim_tx_out_len == 32);
    run(); CHECK(sim_tx_out_len == 100);                  /* el resto, en las vueltas siguientes */

    printf(fails ? "APP: %d FALLOS\n" : "APP: todas las pruebas OK\n", fails);
    return fails != 0;
}
