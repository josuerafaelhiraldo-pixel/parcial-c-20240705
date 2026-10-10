/* pwm.c - PWM por hardware con TIM3 CH1 (PA6). Ver pwm.h para las formulas.
 * La configuracion base (modo PWM1, polaridad alta, PSC = 83) la genera CubeMX
 * en MX_TIM3_Init(); aqui se calculan ARR y CCR y se aplican de forma segura. */
#include "pwm.h"
#include "app_config.h"
#include "tim.h"                      /* htim3, generado por CubeMX */
#include "stm32f4xx_hal.h"

#define PWM_CH   TIM_CHANNEL_1

static uint32_t s_freq;               /* frecuencia aplicada (Hz) */
static uint32_t s_duty;               /* duty aplicado (%) */

/* Reloj de los temporizadores del bus APB1. Regla del microcontrolador: si el
 * divisor de APB1 es distinto de 1, los temporizadores reciben el doble de PCLK1.
 * Con 84 MHz de HCLK y APB1 /2: PCLK1 = 42 MHz -> temporizadores a 84 MHz. */
uint32_t Pwm_TimerClockHz(void)
{
    uint32_t hclk  = HAL_RCC_GetHCLKFreq();
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();

    return (pclk1 == hclk) ? pclk1 : (2U * pclk1);
}

/* Cuentas de TIM3 que dura un periodo de PWM a la frecuencia hz (= ARR + 1). */
static uint32_t counts_per_period(uint32_t hz)
{
    uint32_t tickHz = Pwm_TimerClockHz() / ((uint32_t)htim3.Instance->PSC + 1U);

    return tickHz / hz;
}

/* Escribe ARR y CCR1 sin que el hardware pueda cargar uno solo de los dos:
 * con UDIS puesto no hay actualizacion; al quitarlo, ambos valores entran
 * juntos en el siguiente desbordamiento del contador. */
static void write_regs(uint32_t arr, uint32_t ccr)
{
    SET_BIT(htim3.Instance->CR1, TIM_CR1_UDIS);
    __HAL_TIM_SET_AUTORELOAD(&htim3, arr);
    __HAL_TIM_SET_COMPARE(&htim3, PWM_CH, ccr);
    CLEAR_BIT(htim3.Instance->CR1, TIM_CR1_UDIS);
}

static void apply(uint32_t hz, uint32_t pct)
{
    uint32_t counts = counts_per_period(hz);
    uint32_t ccr = (pct * counts) / 100U;       /* 100 % -> counts = ARR + 1; 0 % -> 0 */

    write_regs(counts - 1U, ccr);
    s_freq = hz;
    s_duty = pct;
}

bool Pwm_FreqIsValid(uint32_t hz)
{
    return (hz == 500U) || (hz == 1000U) || (hz == 2000U);
}

void Pwm_Init(void)
{
    uint32_t counts = counts_per_period(APP_PWM_FREQ_DEFAULT_HZ);

    /* Precarga de ARR y de CCR1: los cambios entran al final del periodo. */
    SET_BIT(htim3.Instance->CR1, TIM_CR1_ARPE);
    __HAL_TIM_ENABLE_OCxPRELOAD(&htim3, PWM_CH);

    /* El contador aun no corre: se escriben los valores y un evento de
     * actualizacion los carga de inmediato en los registros activos. */
    __HAL_TIM_SET_AUTORELOAD(&htim3, counts - 1U);
    __HAL_TIM_SET_COMPARE(&htim3, PWM_CH, (APP_PWM_DUTY_DEFAULT_PCT * counts) / 100U);
    (void)HAL_TIM_GenerateEvent(&htim3, TIM_EVENTSOURCE_UPDATE);
    s_freq = APP_PWM_FREQ_DEFAULT_HZ;
    s_duty = APP_PWM_DUTY_DEFAULT_PCT;

    (void)HAL_TIM_PWM_Start(&htim3, PWM_CH);
}

bool Pwm_SetFreq(uint32_t hz)
{
    if (!Pwm_FreqIsValid(hz)) {
        return false;
    }
    apply(hz, s_duty);
    return true;
}

bool Pwm_SetDuty(uint32_t pct)
{
    if (pct > 100U) {
        return false;
    }
    apply(s_freq, pct);
    return true;
}

bool Pwm_Set(uint32_t hz, uint32_t pct)
{
    if (!Pwm_FreqIsValid(hz) || (pct > 100U)) {
        return false;
    }
    apply(hz, pct);
    return true;
}

uint32_t Pwm_GetFreq(void) { return s_freq; }
uint32_t Pwm_GetDuty(void) { return s_duty; }

void Pwm_GetInfo(PwmInfo *out)
{
    out->timerClkHz = Pwm_TimerClockHz();
    out->prescaler  = (uint32_t)htim3.Instance->PSC;
    out->arr        = __HAL_TIM_GET_AUTORELOAD(&htim3);
    out->ccr        = __HAL_TIM_GET_COMPARE(&htim3, PWM_CH);
    out->freqHz     = s_freq;
    out->dutyPct    = s_duty;
}
