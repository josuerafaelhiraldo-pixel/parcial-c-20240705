/* pwm_check.c - Verificacion del PWM con el medidor interno. Ver pwm_check.h. */
#include "pwm_check.h"
#include "app_config.h"
#include "console.h"
#include "pwm.h"
#include "pwm_meter.h"
#include "tick.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>

typedef enum { CK_IDLE = 0, CK_SETTLE, CK_MEASURE, CK_PRINT, CK_SUMMARY } CkState;

static const uint32_t k_freqs[3]  = { 500U, 1000U, 2000U };
static const uint32_t k_duties[5] = { 0U, 25U, 50U, 75U, 100U };

static CkState      s_st;
static bool         s_all;                 /* true: 15 combinaciones; false: solo la actual */
static uint32_t     s_t0;                  /* inicio de la espera de asentamiento */
static uint32_t     s_idx, s_ok;           /* combinacion actual y cuantas pasaron */
static uint32_t     s_expF, s_expD;        /* lo que se espera medir */
static uint32_t     s_saveF, s_saveD;      /* configuracion previa, para restaurarla */
static uint32_t     s_tim0, s_sys0;        /* TIM6 y SysTick al empezar (comprobacion del tick) */
static uint32_t     s_sumStep;
static bool         s_pass;
static MeterResult  s_res;
static MeterVerdict s_v;

static char sgn(int32_t v) { return (v < 0) ? '-' : '+'; }
static uint32_t mag(int32_t v) { return (v < 0) ? (uint32_t)(-(int64_t)v) : (uint32_t)v; }

static void begin_combo(uint32_t now)
{
    s_expF = k_freqs[s_idx / 5U];
    s_expD = k_duties[s_idx % 5U];
    (void)Pwm_Set(s_expF, s_expD);
    s_t0 = now;
    s_st = CK_SETTLE;
}

bool PwmCheck_Busy(void) { return s_st != CK_IDLE; }

bool PwmCheck_StartOne(uint32_t now)
{
    if (s_st != CK_IDLE) {
        return false;
    }
    s_all  = false;
    s_expF = Pwm_GetFreq();
    s_expD = Pwm_GetDuty();
    s_t0   = now;
    s_st   = CK_SETTLE;
    return true;
}

bool PwmCheck_StartAll(uint32_t now)
{
    if (s_st != CK_IDLE) {
        return false;
    }
    s_all   = true;
    s_idx   = 0U;
    s_ok    = 0U;
    s_saveF = Pwm_GetFreq();
    s_saveD = Pwm_GetDuty();
    s_tim0  = Tick_Ms();
    s_sys0  = HAL_GetTick();
    (void)Console_Write("\r\n--- Autoprueba PWM: TIM3 CH1 (PA6) medido con TIM2 CH1 (PA0), puente PA6-PA0 ---\r\n");
    begin_combo(now);
    return true;
}

/* Imprime la linea de la medicion actual. false = no cupo en la cola TX (se reintenta). */
static bool print_line(void)
{
    char pre[48];

    if (s_all) {
        (void)snprintf(pre, sizeof pre, "[%2lu/%lu]", (unsigned long)(s_idx + 1U), (unsigned long)PWM_CHECK_COMBOS);
    } else {
        (void)snprintf(pre, sizeof pre, "[medida]");
    }

    if (!s_res.hasEdges) {
        return Console_Printf("%s %4lu Hz duty %3lu %% -> sin flancos, nivel %s, capturas=%lu   %s\r\n",
                              pre, (unsigned long)s_expF, (unsigned long)s_expD,
                              s_res.levelHigh ? "ALTO" : "BAJO", (unsigned long)s_res.captures,
                              s_pass ? "OK" : "FALLA");
    }
    return Console_Printf("%s %4lu Hz duty %3lu %% -> f=%lu.%lu Hz (%c%lu.%02lu %%) duty=%lu.%02lu %% (%c%lu.%02lu pp) n=%lu   %s\r\n",
                          pre, (unsigned long)s_expF, (unsigned long)s_expD,
                          (unsigned long)(s_res.freqMilliHz / 1000U),
                          (unsigned long)((s_res.freqMilliHz % 1000U) / 100U),
                          sgn(s_v.freqErrCenti), (unsigned long)(mag(s_v.freqErrCenti) / 100U),
                          (unsigned long)(mag(s_v.freqErrCenti) % 100U),
                          (unsigned long)(s_res.dutyCentiPct / 100U),
                          (unsigned long)(s_res.dutyCentiPct % 100U),
                          sgn(s_v.dutyErrCenti), (unsigned long)(mag(s_v.dutyErrCenti) / 100U),
                          (unsigned long)(mag(s_v.dutyErrCenti) % 100U),
                          (unsigned long)s_res.samples, s_pass ? "OK" : "FALLA");
}

/* Resumen final en tres mensajes; cada uno solo avanza si cupo en la cola. */
static bool print_summary(void)
{
    uint32_t dTim;
    uint32_t dSys;
    uint32_t tickHz = Pwm_TimerClockHz();

    switch (s_sumStep) {
    case 0U:
        return Console_Printf("Resultado: %lu/%lu OK  (reloj TIM=%lu Hz, resolucion %lu.%lu ns, ventana %lu ms)\r\n",
                              (unsigned long)s_ok, (unsigned long)PWM_CHECK_COMBOS,
                              (unsigned long)tickHz,
                              (unsigned long)((uint32_t)(10000000000ULL / tickHz) / 10U),
                              (unsigned long)((uint32_t)(10000000000ULL / tickHz) % 10U),
                              (unsigned long)APP_METER_WINDOW_MS);
    case 1U:
        dTim = (uint32_t)(Tick_Ms() - s_tim0);
        dSys = (uint32_t)(HAL_GetTick() - s_sys0);
        return Console_Printf("Tick durante la autoprueba: TIM6 +%lu ms, SysTick +%lu ms, diferencia %ld ms\r\n",
                              (unsigned long)dTim, (unsigned long)dSys, (long)(int32_t)(dTim - dSys));
    default:
        return Console_Write("Nota: medicion interna; TIM2 y TIM3 comparten reloj, no sustituye a un instrumento.\r\n");
    }
}

void PwmCheck_Service(uint32_t now)
{
    switch (s_st) {
    case CK_SETTLE:
        if ((uint32_t)(now - s_t0) >= APP_CHECK_SETTLE_MS) {
            Meter_Start(now);
            s_st = CK_MEASURE;
        }
        break;

    case CK_MEASURE:
        if (Meter_Service(now)) {
            Meter_GetResult(&s_res);
            s_pass = Meter_Judge(&s_res, s_expF, s_expD, &s_v);
            s_st = CK_PRINT;
        }
        break;

    case CK_PRINT:
        if (print_line()) {
            if (s_pass) {
                s_ok++;
            }
            if (!s_all) {
                s_st = CK_IDLE;
            } else {
                s_idx++;
                if (s_idx < PWM_CHECK_COMBOS) {
                    begin_combo(now);
                } else {
                    (void)Pwm_Set(s_saveF, s_saveD);         /* restaura lo que habia */
                    s_sumStep = 0U;
                    s_st = CK_SUMMARY;
                }
            }
        }
        break;

    case CK_SUMMARY:
        if (print_summary()) {
            s_sumStep++;
            if (s_sumStep >= 3U) {
                s_st = CK_IDLE;
            }
        }
        break;

    default:
        break;
    }
}
