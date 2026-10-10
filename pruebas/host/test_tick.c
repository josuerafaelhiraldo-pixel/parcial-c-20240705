/* Pruebas en PC de tick.c: contador de 1 ms y desbordamiento de uint32. */
#include "tick.h"
#include "hal_sim.h"
#include <stdio.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALLO linea %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void)
{
    uint32_t t0, t1;

    Tick_Init();
    CHECK(sim_tim_start_calls == 1);          /* arranca TIM6 con interrupcion, una vez */
    CHECK(sim_tim_flag_clears == 1);          /* borra UIF antes de arrancar */
    CHECK(Tick_Ms() == 0U);

    sim_tick(1000);
    CHECK(Tick_Ms() == 1000U);                /* 1000 interrupciones = 1000 ms */

    /* otro temporizador (TIM3, PWM) no debe contar */
    HAL_TIM_PeriodElapsedCallback(&htim3);
    CHECK(Tick_Ms() == 1000U);

    /* desbordamiento: la diferencia sin signo sigue bien */
    Tick_SetMs(0xFFFFFFFEU);
    t0 = Tick_Ms();
    sim_tick(5);
    t1 = Tick_Ms();
    CHECK(t1 == 3U);                          /* 0xFFFFFFFE + 5 = 3 (vuelta) */
    CHECK((uint32_t)(t1 - t0) == 5U);
    CHECK(t1 < t0);                           /* la comparacion directa FALLARIA: por eso se resta */

    /* un intervalo de 1500 ms que cruza el desbordamiento */
    Tick_SetMs(0xFFFFFE00U);
    t0 = Tick_Ms();
    sim_tick(1499);
    CHECK((uint32_t)(Tick_Ms() - t0) == 1499U);
    CHECK((uint32_t)(Tick_Ms() - t0) < 1500U);
    sim_tick(1);
    CHECK((uint32_t)(Tick_Ms() - t0) >= 1500U);

    printf(fails ? "TICK: %d FALLOS\n" : "TICK: todas las pruebas OK\n", fails);
    return fails != 0;
}
