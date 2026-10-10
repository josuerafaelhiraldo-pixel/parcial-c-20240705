/* Pruebas en PC de pwm_meter.c con un modelo del hardware (hal_sim.c):
 * configuracion de TIM2, mediciones, niveles constantes, veredicto y fallos. */
#include "pwm.h"
#include "pwm_meter.h"
#include "hal_sim.h"
#include <stdio.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALLO linea %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void fw_start(uint32_t now) { sim_tim2_enter(); Meter_Start(now); sim_tim2_leave(); }
static bool fw_service(uint32_t now) { sim_tim2_enter(); bool r = Meter_Service(now); sim_tim2_leave(); return r; }

static int g_iters;                    /* milisegundos que duro la medicion */

/* Una medicion completa: 10 vueltas de main por milisegundo (100 us cada una). */
static MeterResult measure(uint32_t t0)
{
    MeterResult r = { 0 };
    uint32_t now = t0;

    fw_start(t0);
    g_iters = 0;
    for (;;) {
        for (int k = 0; k < 10; k++) {
            sim_pwm_run_us(100);
            if (fw_service(now)) { Meter_GetResult(&r); return r; }
        }
        now++;
        g_iters++;
        if (g_iters > 2000) { CHECK(!"la medicion no termina"); return r; }
    }
}

static MeterVerdict judge(const MeterResult *r, uint32_t f, uint32_t d, bool *pass)
{
    MeterVerdict v;
    *pass = Meter_Judge(r, f, d, &v);
    return v;
}

int main(void)
{
    const uint32_t freqs[3] = { 500U, 1000U, 2000U };
    MeterResult r;
    MeterVerdict v;
    bool pass;

    sim_reset_pwm_model();
    Pwm_Init();
    Meter_Init();

    /* configuracion de PA0 y de TIM2 (RM0390) */
    CHECK(sim_gpio_init_calls == 1);
    CHECK(sim_gpio_init_last.Pin == GPIO_PIN_0 && sim_gpio_init_last.Mode == GPIO_MODE_AF_PP);
    CHECK(sim_gpio_init_last.Pull == GPIO_PULLDOWN && sim_gpio_init_last.Alternate == GPIO_AF1_TIM2);
    CHECK(sim_TIM2.SMCR == 0x0054U);        /* TS = 101 (TI1FP1), SMS = 100 (reset) */
    CHECK(sim_TIM2.CCMR1 == 0x0201U);       /* CC1S = 01, CC2S = 10 */
    CHECK(sim_TIM2.CCER == 0x0031U);        /* CC1E, CC2E, CC2P (bajada) */
    CHECK(sim_TIM2.PSC == 0U && sim_TIM2.ARR == 0xFFFFFFFFU);
    CHECK(sim_TIM2.CR1 == TIM_CR1_CEN);
    CHECK(Meter_State() == METER_IDLE);
    CHECK(!fw_service(0U));                 /* sin Start no hay nada que servir */

    /* 25 / 50 / 75 % a las tres frecuencias: medida exacta y veredicto OK */
    for (unsigned i = 0; i < 3; i++) {
        for (uint32_t d = 25U; d <= 75U; d += 25U) {
            uint32_t perExp = (uint32_t)(84000000ULL / freqs[i]);
            CHECK(Pwm_Set(freqs[i], d));
            r = measure(1000U);
            CHECK(Meter_State() == METER_DONE);
            CHECK(g_iters == 200);                                   /* ventana de 200 ms */
            CHECK(r.hasEdges && r.tickHz == 84000000U);
            CHECK(r.periodTicks == perExp);
            CHECK(r.freqMilliHz == freqs[i] * 1000U);
            CHECK(r.dutyCentiPct == d * 100U);
            CHECK(r.samples >= (freqs[i] * 200U / 1000U) * 95U / 100U && r.samples <= freqs[i] * 200U / 1000U);
            CHECK(r.missed == 0U);
            v = judge(&r, freqs[i], d, &pass);
            CHECK(pass && v.freqOk && v.dutyOk && !v.constant);
            CHECK(v.freqErrCenti == 0 && v.dutyErrCenti == 0);
        }
    }

    /* 0 % y 100 %: sin flancos en toda la ventana, nivel correcto */
    for (unsigned i = 0; i < 3; i++) {
        CHECK(Pwm_Set(freqs[i], 0U));
        r = measure(5000U);
        CHECK(!r.hasEdges && !r.levelHigh && r.captures == 0U);
        v = judge(&r, freqs[i], 0U, &pass);
        CHECK(pass && v.constant && v.levelOk);
        v = judge(&r, freqs[i], 100U, &pass);                        /* bajo no vale como 100 % */
        CHECK(!pass);

        CHECK(Pwm_Set(freqs[i], 100U));
        r = measure(6000U);
        CHECK(!r.hasEdges && r.levelHigh && r.captures == 0U);
        v = judge(&r, freqs[i], 100U, &pass);
        CHECK(pass && v.levelOk);
        v = judge(&r, freqs[i], 0U, &pass);                          /* alto no vale como 0 % */
        CHECK(!pass);
    }

    /* 99 % y 1 % NO son niveles constantes: tienen flancos y se miden */
    CHECK(Pwm_Set(1000U, 99U)); r = measure(7000U);
    CHECK(r.hasEdges && r.dutyCentiPct == 9900U);
    CHECK(Pwm_Set(1000U, 1U));  r = measure(7000U);
    CHECK(r.hasEdges && r.dutyCentiPct == 100U);

    /* las banderas viejas se borran al empezar: un flanco previo no cuenta en 0 % */
    CHECK(Pwm_Set(1000U, 25U)); sim_pwm_edge();                      /* captura sin atender */
    CHECK(Pwm_Set(1000U, 0U));
    r = measure(8000U);
    CHECK(!r.hasEdges && r.captures == 0U);
    v = judge(&r, 1000U, 0U, &pass); CHECK(pass);

    /* error de frecuencia: la tolerancia es de 2,00 % */
    CHECK(Pwm_Set(1000U, 50U));
    sim_period_error_ppm = 20000;                                    /* periodo +2,0 % -> f = -1,96 % */
    r = measure(9000U); v = judge(&r, 1000U, 50U, &pass);
    CHECK(pass && v.freqOk && v.freqErrCenti > -200 && v.freqErrCenti < -190);
    sim_period_error_ppm = 21000;                                    /* f = -2,06 % */
    r = measure(9000U); v = judge(&r, 1000U, 50U, &pass);
    CHECK(!pass && !v.freqOk && v.dutyOk);
    sim_period_error_ppm = -21000;                                   /* periodo -2,1 % -> f = +2,15 % */
    r = measure(9000U); v = judge(&r, 1000U, 50U, &pass);
    CHECK(!pass && !v.freqOk && v.freqErrCenti > 200);
    sim_period_error_ppm = 0;

    /* frecuencia mal programada (pedir 1 kHz y tener 500 Hz): se detecta */
    CHECK(Pwm_Set(500U, 50U));
    r = measure(9500U); v = judge(&r, 1000U, 50U, &pass);
    CHECK(!pass && !v.freqOk && v.freqErrCenti == -5000);
    /* duty mal programado: se detecta (esperar 50 % con 25 %) */
    CHECK(Pwm_Set(1000U, 25U));
    r = measure(9600U); v = judge(&r, 1000U, 50U, &pass);
    CHECK(!pass && v.freqOk && !v.dutyOk && v.dutyErrCenti == -2500);
    /* borde de la tolerancia en duty: +-2 puntos pasan, +-3 no (esperar 27 y 28 con 25) */
    v = judge(&r, 1000U, 27U, &pass); CHECK(pass && v.dutyOk && v.dutyErrCenti == -200);
    v = judge(&r, 1000U, 28U, &pass); CHECK(!pass && !v.dutyOk);
    v = judge(&r, 1000U, 23U, &pass); CHECK(pass && v.dutyOk && v.dutyErrCenti == 200);
    v = judge(&r, 1000U, 22U, &pass); CHECK(!pass && !v.dutyOk);

    /* puente suelto: PA0 queda en 0 -> 25/50/75 y 100 % fallan; 0 % pasa (por eso se prueban todos) */
    sim_pwm_disconnected = 1;
    CHECK(Pwm_Set(1000U, 25U));  r = measure(10000U);
    CHECK(!r.hasEdges && !r.levelHigh); v = judge(&r, 1000U, 25U, &pass); CHECK(!pass);
    CHECK(Pwm_Set(1000U, 100U)); r = measure(10000U);
    CHECK(!r.levelHigh); v = judge(&r, 1000U, 100U, &pass); CHECK(!pass);
    CHECK(Pwm_Set(1000U, 0U));   r = measure(10000U);
    v = judge(&r, 1000U, 0U, &pass); CHECK(pass);
    sim_pwm_disconnected = 0;

    /* pulsos residuales: un nivel "constante" con alguna captura NO pasa */
    {
        MeterResult rr = { 0 };
        rr.hasEdges = false; rr.levelHigh = false; rr.captures = 1U;
        v = judge(&rr, 1000U, 0U, &pass); CHECK(!pass);
        rr.captures = 0U;
        v = judge(&rr, 1000U, 0U, &pass); CHECK(pass);
        rr.hasEdges = true; rr.samples = 50U; rr.freqMilliHz = 1000000U; rr.dutyCentiPct = 0U;
        v = judge(&rr, 1000U, 0U, &pass); CHECK(!pass);              /* con flancos no es 0 % */
        /* pocas muestras: no se acepta como medida valida */
        rr.samples = 9U; rr.dutyCentiPct = 5000U;
        v = judge(&rr, 1000U, 50U, &pass); CHECK(!pass);
        rr.samples = 10U;
        v = judge(&rr, 1000U, 50U, &pass); CHECK(pass);
    }

    /* capturas no validas (tiempo en alto mayor que el periodo, o periodo 0): no se usan */
    CHECK(Pwm_Set(1000U, 0U));
    {
        uint32_t now = 30000U;
        fw_start(now);
        for (int i = 0; i < 10; i++) {                                /* 6 invalidas con 2 descartadas */
            sim_inject_capture((i % 2) ? 0U : 1000U, 2000U);
            now++;
            fw_service(now);
        }
        while (!fw_service(++now)) { }
        Meter_GetResult(&r);
        CHECK(r.samples == 0U && !r.hasEdges && r.captures == 10U);
        v = judge(&r, 1000U, 0U, &pass); CHECK(!pass);               /* 0 % con capturas: no pasa */
    }

    /* sobrecaptura: si main atiende cada 3 ms habra capturas perdidas, pero el valor
     * leido sigue siendo un periodo completo y correcto */
    CHECK(Pwm_Set(2000U, 50U));
    {
        uint32_t now = 20000U;
        fw_start(now);
        for (;;) {
            sim_pwm_run_us(1000); sim_pwm_run_us(1000); sim_pwm_run_us(1000);
            now += 3U;
            if (fw_service(now)) break;
        }
        Meter_GetResult(&r);
        CHECK(r.missed > 0U);
        CHECK(r.hasEdges && r.freqMilliHz == 2000000U && r.dutyCentiPct == 5000U);
    }

    /* tiempo cerca de UINT32_MAX: la ventana dura lo mismo al cruzar el desbordamiento */
    CHECK(Pwm_Set(1000U, 25U));
    r = measure(0xFFFFFF00U);
    CHECK(g_iters == 200 && r.hasEdges && r.dutyCentiPct == 2500U);
    r = measure(0xFFFFFFFFU - 99U);
    CHECK(g_iters == 200);

    /* un solo Service devuelve true al terminar; despues ya no */
    fw_start(0U);
    CHECK(Meter_State() == METER_RUNNING);
    CHECK(!fw_service(199U));
    CHECK(fw_service(200U));
    CHECK(Meter_State() == METER_DONE);
    CHECK(!fw_service(201U));

    printf(fails ? "MEDIDOR: %d FALLOS\n" : "MEDIDOR: todas las pruebas OK\n", fails);
    return fails != 0;
}
