/* Pruebas en PC de pwm.c: formulas de ARR/CCR, niveles 0 % y 100 %, rangos,
 * reloj real, y escritura segura (UDIS) de los registros. */
#include "pwm.h"
#include "hal_sim.h"
#include <stdio.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALLO linea %d: %s\n", __LINE__, #c); fails++; } } while (0)

static uint32_t arr(void) { return sim_TIM3.ARR; }
static uint32_t ccr(void) { return sim_TIM3.CCR1; }

int main(void)
{
    PwmInfo pi;
    const uint32_t freqs[3] = { 500U, 1000U, 2000U };
    uint32_t d;

    sim_reset_pwm_model();
    Pwm_Init();
    /* arranque: 1 kHz, 25 % con PSC = 83 y reloj de 84 MHz -> 1 MHz de cuenta */
    CHECK(Pwm_TimerClockHz() == 84000000U);
    CHECK(arr() == 999U && ccr() == 250U);
    CHECK(Pwm_GetFreq() == 1000U && Pwm_GetDuty() == 25U);
    CHECK((sim_TIM3.CR1 & TIM_CR1_ARPE) != 0U);           /* ARR con precarga */
    CHECK(sim_oc_preload_enabled == 1);                    /* CCR1 con precarga */
    CHECK(sim_ug_calls == 1 && sim_ug_before_start == 1);  /* un UG, con el contador detenido */
    CHECK(sim_pwm_start_calls == 1 && (sim_TIM3.CR1 & TIM_CR1_CEN) != 0U);
    CHECK((sim_TIM3.CR1 & TIM_CR1_UDIS) == 0U);
    CHECK(sim_unsafe_writes == 0);

    Pwm_GetInfo(&pi);
    CHECK(pi.timerClkHz == 84000000U && pi.prescaler == 83U && pi.arr == 999U && pi.ccr == 250U);
    CHECK(pi.freqHz == 1000U && pi.dutyPct == 25U);

    /* frecuencias: ARR = 1999 / 999 / 499; el duty se conserva (25 %) */
    CHECK(Pwm_SetFreq(500U));  CHECK(arr() == 1999U && ccr() == 500U);
    CHECK(Pwm_SetFreq(2000U)); CHECK(arr() == 499U  && ccr() == 125U);
    CHECK(Pwm_SetFreq(1000U)); CHECK(arr() == 999U  && ccr() == 250U);

    /* frecuencias no permitidas: false y nada cambia */
    {
        const uint32_t bad[] = { 0U, 1U, 499U, 501U, 750U, 999U, 1001U, 1500U, 1999U, 2001U, 4000U, 0xFFFFFFFFU };
        for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            CHECK(!Pwm_SetFreq(bad[i]));
            CHECK(arr() == 999U && ccr() == 250U && Pwm_GetFreq() == 1000U);
        }
    }

    /* duty: exacto para todos los enteros 0..100 en las tres frecuencias:
     * CCR * 100 == duty * (ARR + 1), sin redondeo */
    for (unsigned i = 0; i < 3; i++) {
        CHECK(Pwm_SetFreq(freqs[i]));
        for (d = 0; d <= 100U; d++) {
            CHECK(Pwm_SetDuty(d));
            CHECK((uint64_t)ccr() * 100U == (uint64_t)d * (arr() + 1U));
            CHECK(Pwm_GetDuty() == d && Pwm_GetFreq() == freqs[i]);
        }
    }

    /* 0 %: CCR = 0 (salida baja constante); 100 %: CCR = ARR + 1 (alta constante) */
    for (unsigned i = 0; i < 3; i++) {
        CHECK(Pwm_SetFreq(freqs[i]));
        CHECK(Pwm_SetDuty(0U));   CHECK(ccr() == 0U);
        CHECK(Pwm_SetDuty(100U)); CHECK(ccr() == arr() + 1U);
        CHECK(Pwm_SetDuty(99U));  CHECK(ccr() < arr() + 1U);     /* 99 % NO es constante */
        CHECK(Pwm_SetDuty(1U));   CHECK(ccr() > 0U);
    }

    /* duty fuera de rango: false y sin cambios */
    CHECK(Pwm_SetDuty(50U));
    {
        const uint32_t bad[] = { 101U, 200U, 1000U, 0xFFFFFFFFU };
        for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            uint32_t a = arr(), c = ccr();
            CHECK(!Pwm_SetDuty(bad[i]));
            CHECK(arr() == a && ccr() == c && Pwm_GetDuty() == 50U);
        }
    }

    /* cambiar la frecuencia con duty 100 %: sigue en 100 % (CCR = nuevo ARR + 1) */
    CHECK(Pwm_SetDuty(100U));
    CHECK(Pwm_SetFreq(500U));  CHECK(ccr() == arr() + 1U);
    CHECK(Pwm_SetFreq(2000U)); CHECK(ccr() == arr() + 1U);
    CHECK(Pwm_SetDuty(0U));
    CHECK(Pwm_SetFreq(1000U)); CHECK(ccr() == 0U);

    /* Pwm_Set: ambos a la vez; si uno es invalido no cambia ninguno */
    CHECK(Pwm_Set(500U, 75U)); CHECK(arr() == 1999U && ccr() == 1500U);
    CHECK(!Pwm_Set(750U, 10U)); CHECK(!Pwm_Set(500U, 101U));
    CHECK(arr() == 1999U && ccr() == 1500U && Pwm_GetFreq() == 500U && Pwm_GetDuty() == 75U);

    /* escritura segura: todas las escrituras de ARR/CCR con el contador en marcha
     * se hicieron con UDIS puesto, y UDIS quedo borrado al final */
    CHECK(sim_arr_writes > 100 && sim_ccr_writes > 100);
    CHECK(sim_unsafe_writes == 0);
    CHECK((sim_TIM3.CR1 & TIM_CR1_UDIS) == 0U);
    CHECK((sim_TIM3.CR1 & TIM_CR1_CEN) != 0U);

    /* el reloj se toma de RCC, no se supone: otro arbol de relojes da otro ARR */
    sim_hclk = 72000000U; sim_pclk1 = 36000000U;           /* timers a 72 MHz -> 857 142 cuentas/s */
    CHECK(Pwm_TimerClockHz() == 72000000U);
    CHECK(Pwm_SetFreq(500U)); CHECK(arr() == 1713U);       /* 857142 / 500 = 1714 cuentas */
    sim_hclk = 84000000U; sim_pclk1 = 84000000U;           /* APB1 sin dividir: timers = PCLK1 */
    CHECK(Pwm_TimerClockHz() == 84000000U);
    sim_hclk = 168000000U; sim_pclk1 = 42000000U;          /* APB1 /4: timers = 2 x PCLK1 */
    CHECK(Pwm_TimerClockHz() == 84000000U);
    sim_hclk = 84000000U; sim_pclk1 = 42000000U;
    CHECK(Pwm_SetFreq(1000U)); CHECK(arr() == 999U);

    /* el preescalador tambien se lee del registro: PSC = 41 -> 2 MHz de cuenta -> ARR = 1999 a 1 kHz */
    sim_TIM3.PSC = 41U;
    CHECK(Pwm_SetFreq(1000U)); CHECK(arr() == 1999U);
    CHECK(Pwm_SetFreq(2000U)); CHECK(arr() == 999U);
    Pwm_GetInfo(&pi); CHECK(pi.prescaler == 41U);
    sim_TIM3.PSC = 83U;
    CHECK(Pwm_SetFreq(1000U)); CHECK(arr() == 999U);

    printf(fails ? "PWM: %d FALLOS\n" : "PWM: todas las pruebas OK\n", fails);
    return fails != 0;
}
