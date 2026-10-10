/* pwm_meter.c - Medidor interno del PWM con TIM2 (modo entrada PWM). Ver pwm_meter.h.
 *
 * Los registros de TIM2 se configuran aqui de forma explicita (RM0390, TIM2/TIM5):
 *   SMCR : TS = 101 (TI1FP1), SMS = 100 (modo esclavo "reset")
 *   CCMR1: CC1S = 01 (CH1 <- TI1),  CC2S = 10 (CH2 <- TI1, entrada indirecta)
 *   CCER : CC1E, CC2E, CC2P (CH1 por flanco de subida, CH2 por flanco de bajada)
 * Asi el resultado no depende de como haya quedado la configuracion de CubeMX. */
#include "pwm_meter.h"
#include "app_config.h"
#include "pwm.h"
#include "stm32f4xx_hal.h"

static volatile MeterState s_state;       /* solo main */
static uint32_t s_start;
static uint32_t s_skip, s_n, s_raw, s_missed;
static uint64_t s_sumPer, s_sumHi;
static MeterResult s_res;

void Meter_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();

    /* PA0 = TIM2_CH1 (AF1). Pull-down: sin puente, la entrada lee 0 y las
     * pruebas de 25/50/75 % y de 100 % fallan en vez de dar un falso OK. */
    g.Pin       = APP_METER_PIN;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_PULLDOWN;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(APP_METER_PORT, &g);

    TIM2->CR1   = 0U;                                         /* detenido mientras se configura */
    TIM2->PSC   = 0U;                                         /* una cuenta por ciclo de reloj del temporizador */
    TIM2->ARR   = 0xFFFFFFFFU;
    TIM2->SMCR  = TIM_SMCR_TS_2 | TIM_SMCR_TS_0 | TIM_SMCR_SMS_2;
    TIM2->CCMR1 = TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_1;
    TIM2->CCER  = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC2P;
    TIM2->EGR   = TIM_EGR_UG;                                 /* carga el preescalador */
    TIM2->SR    = 0U;
    TIM2->CR1   = TIM_CR1_CEN;

    s_state = METER_IDLE;
}

void Meter_Start(uint32_t now)
{
    s_start  = now;
    s_skip   = APP_METER_DISCARD;
    s_n      = 0U;
    s_raw    = 0U;
    s_missed = 0U;
    s_sumPer = 0U;
    s_sumHi  = 0U;
    (void)TIM2->CCR1;                                         /* leer CCR1 borra CC1IF */
    TIM2->SR = ~(TIM_SR_CC1IF | TIM_SR_CC1OF | TIM_SR_CC2IF | TIM_SR_CC2OF);
    s_state  = METER_RUNNING;
}

static void finish(void)
{
    s_res.samples  = s_n;
    s_res.captures = s_raw;
    s_res.missed   = s_missed;
    s_res.tickHz   = Pwm_TimerClockHz() / ((uint32_t)TIM2->PSC + 1U);

    if (s_n > 0U) {
        s_res.hasEdges     = true;
        s_res.levelHigh    = false;
        s_res.periodTicks  = (uint32_t)((s_sumPer + (s_n / 2U)) / s_n);
        s_res.freqMilliHz  = (uint32_t)(((uint64_t)s_res.tickHz * s_n * 1000ULL) / s_sumPer);
        s_res.dutyCentiPct = (uint32_t)((s_sumHi * 10000ULL) / s_sumPer);
    } else {
        s_res.hasEdges     = false;
        s_res.levelHigh    = (HAL_GPIO_ReadPin(APP_METER_PORT, APP_METER_PIN) == GPIO_PIN_SET);
        s_res.periodTicks  = 0U;
        s_res.freqMilliHz  = 0U;
        s_res.dutyCentiPct = 0U;
    }
    s_state = METER_DONE;
}

bool Meter_Service(uint32_t now)
{
    uint32_t sr;
    uint32_t per;
    uint32_t hi;

    if (s_state != METER_RUNNING) {
        return false;
    }

    sr = TIM2->SR;
    if ((sr & TIM_SR_CC1IF) != 0U) {
        uint32_t clr = TIM_SR_CC1IF;

        per = TIM2->CCR1;                                     /* periodo (cuentas) */
        hi  = TIM2->CCR2;                                     /* tiempo en alto (cuentas) */
        s_raw++;
        if ((sr & TIM_SR_CC1OF) != 0U) {
            s_missed++;                                       /* hubo mas de una captura entre dos lecturas */
            clr |= TIM_SR_CC1OF;
        }
        TIM2->SR = ~clr;                                      /* las banderas se borran escribiendo 0 */
        if (s_skip > 0U) {
            s_skip--;                                         /* las primeras capturas pueden ser de un periodo anterior */
        } else if ((per != 0U) && (hi <= per) && (s_n < APP_METER_MAX_SAMPLES)) {
            s_sumPer += per;
            s_sumHi  += hi;
            s_n++;
        } else {
            /* captura no valida o tope alcanzado: se ignora */
        }
    }

    if ((uint32_t)(now - s_start) >= APP_METER_WINDOW_MS) {   /* resta sin signo: resiste el desbordamiento */
        finish();
        return true;
    }
    return false;
}

MeterState Meter_State(void) { return s_state; }

void Meter_GetResult(MeterResult *out) { *out = s_res; }

static int32_t abs32(int32_t v) { return (v < 0) ? -v : v; }

bool Meter_Judge(const MeterResult *r, uint32_t expFreqHz, uint32_t expDutyPct, MeterVerdict *v)
{
    int64_t expMilli = (int64_t)expFreqHz * 1000;

    v->pass         = false;
    v->constant     = (expDutyPct == 0U) || (expDutyPct >= 100U);
    v->levelOk      = false;
    v->freqOk       = false;
    v->dutyOk       = false;
    v->freqErrCenti = 0;
    v->dutyErrCenti = 0;

    if (v->constant) {
        /* 0 % o 100 %: ningun flanco en toda la ventana (sin pulsos residuales)
         * y el nivel correcto. */
        bool wantHigh = (expDutyPct >= 100U);

        v->levelOk = (!r->hasEdges) && (r->captures == 0U) && (r->levelHigh == wantHigh);
        v->pass    = v->levelOk;
        return v->pass;
    }

    if ((!r->hasEdges) || (r->samples < APP_METER_MIN_SAMPLES)) {
        return false;
    }

    v->freqErrCenti = (int32_t)((((int64_t)r->freqMilliHz - expMilli) * 10000) / expMilli);
    v->dutyErrCenti = (int32_t)((int64_t)r->dutyCentiPct - ((int64_t)expDutyPct * 100));
    v->freqOk = (abs32(v->freqErrCenti) <= APP_METER_FREQ_TOL_CENTI);
    v->dutyOk = (abs32(v->dutyErrCenti) <= APP_METER_DUTY_TOL_CENTI);
    v->pass   = v->freqOk && v->dutyOk;
    return v->pass;
}
