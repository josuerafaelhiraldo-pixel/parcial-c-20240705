/* ctrl.c - Maquina de estados MANUAL / AUTO / PAUSA. Ver ctrl.h. */
#include "ctrl.h"
#include "app_config.h"
#include "pwm.h"
#include "stm32f4xx_hal.h"

static CtrlMode s_mode;         /* estado actual */
static CtrlMode s_resumeTo;     /* estado al que vuelve RESUME */
static uint32_t s_manual;       /* ultimo duty manual */
static uint32_t s_ramp;         /* valor de la rampa */
static int8_t   s_dir;          /* direccion de la rampa */
static bool     s_ledOn;
static uint32_t s_ledLast;      /* instante del ultimo cambio del patron del LED */
static uint32_t s_rampLast;     /* instante del ultimo paso de la rampa */
static uint32_t s_shortIgnored;

static void led_write(bool on)
{
    s_ledOn = on;
    HAL_GPIO_WritePin(APP_LED_PORT, APP_LED_PIN, on ? APP_LED_ON_LEVEL : APP_LED_OFF_LEVEL);
}

/* Al entrar en un estado (o al reanudar): el LED empieza encendido y los temporizadores de
 * patron y de rampa empiezan desde ahora. Asi no se "recuperan" pasos del tiempo pausado. */
static void restart_pattern(uint32_t now)
{
    s_ledLast  = now;
    s_rampLast = now;
    led_write(true);
}

void Ctrl_Init(uint32_t now)
{
    s_mode         = MODE_MANUAL;
    s_resumeTo     = MODE_MANUAL;
    s_manual       = APP_PWM_DUTY_DEFAULT_PCT;
    s_ramp         = APP_RAMP_MIN_PCT;
    s_dir          = 1;
    s_shortIgnored = 0U;
    (void)Pwm_Set(APP_PWM_FREQ_DEFAULT_HZ, s_manual);
    restart_pattern(now);
}

static void ramp_step(void)
{
    if (s_dir > 0) {
        s_ramp++;
        if (s_ramp >= APP_RAMP_MAX_PCT) {
            s_ramp = APP_RAMP_MAX_PCT;
            s_dir  = -1;                /* en el tope: empieza a bajar */
        }
    } else {
        s_ramp--;
        if (s_ramp <= APP_RAMP_MIN_PCT) {
            s_ramp = APP_RAMP_MIN_PCT;
            s_dir  = 1;                 /* en el piso: empieza a subir */
        }
    }
    (void)Pwm_SetDuty(s_ramp);
}

void Ctrl_Service(uint32_t now)
{
    uint32_t period = 0U;

    if (s_mode == MODE_AUTO) {
        period = APP_LED_AUTO_MS;
    } else if (s_mode == MODE_PAUSE) {
        period = APP_LED_PAUSE_MS;
    } else {
        /* MANUAL: LED fijo */
    }

    if ((period != 0U) && ((uint32_t)(now - s_ledLast) >= period)) {
        s_ledLast += period;            /* se avanza el periodo exacto: sin deriva */
        led_write(!s_ledOn);
    }

    if ((s_mode == MODE_AUTO) && ((uint32_t)(now - s_rampLast) >= APP_RAMP_STEP_MS)) {
        s_rampLast += APP_RAMP_STEP_MS;
        ramp_step();
    }
}

CtrlResult Ctrl_SetManual(uint32_t now)
{
    if (s_mode == MODE_PAUSE) {
        return CTRL_ERR_STATE;
    }
    if (s_mode != MODE_MANUAL) {
        s_mode = MODE_MANUAL;
        restart_pattern(now);
    }
    (void)Pwm_SetDuty(s_manual);        /* restaura el ultimo duty manual */
    return CTRL_OK;
}

CtrlResult Ctrl_SetAuto(uint32_t now)
{
    if (s_mode == MODE_PAUSE) {
        return CTRL_ERR_STATE;
    }
    s_mode = MODE_AUTO;
    s_ramp = APP_RAMP_MIN_PCT;          /* siempre desde 10 % y ascendiendo */
    s_dir  = 1;
    (void)Pwm_SetDuty(s_ramp);
    restart_pattern(now);
    return CTRL_OK;
}

CtrlResult Ctrl_SetDuty(uint32_t pct)
{
    if (s_mode != MODE_MANUAL) {
        return CTRL_ERR_STATE;          /* AUTO y PAUSA lo rechazan */
    }
    if (pct > 100U) {
        return CTRL_ERR_RANGE;
    }
    s_manual = pct;
    (void)Pwm_SetDuty(pct);
    return CTRL_OK;
}

CtrlResult Ctrl_SetFreq(uint32_t hz)
{
    if (s_mode == MODE_PAUSE) {
        return CTRL_ERR_STATE;
    }
    if (!Pwm_SetFreq(hz)) {             /* conserva el duty aplicado */
        return CTRL_ERR_RANGE;
    }
    return CTRL_OK;
}

CtrlResult Ctrl_Pause(uint32_t now)
{
    if (s_mode == MODE_PAUSE) {
        return CTRL_OK;                 /* ya pausado: no se toca lo guardado */
    }
    s_resumeTo = s_mode;
    s_mode     = MODE_PAUSE;
    (void)Pwm_SetDuty(0U);              /* salida constante en bajo; s_manual y s_ramp no se tocan */
    restart_pattern(now);
    return CTRL_OK;
}

CtrlResult Ctrl_Resume(uint32_t now)
{
    if (s_mode != MODE_PAUSE) {
        return CTRL_ERR_STATE;
    }
    s_mode = s_resumeTo;
    (void)Pwm_SetDuty((s_mode == MODE_AUTO) ? s_ramp : s_manual);
    restart_pattern(now);               /* la rampa sigue desde su valor y direccion guardados */
    return CTRL_OK;
}

bool Ctrl_OnButton(ButtonEvent ev, uint32_t now)
{
    if (ev == BTN_EVT_SHORT) {
        if (s_mode == MODE_MANUAL) {
            (void)Ctrl_SetAuto(now);
            return true;
        }
        if (s_mode == MODE_AUTO) {
            (void)Ctrl_SetManual(now);
            return true;
        }
        s_shortIgnored++;               /* PAUSA: la corta se ignora */
        return false;
    }
    if (ev == BTN_EVT_LONG) {
        if (s_mode == MODE_PAUSE) {
            (void)Ctrl_Resume(now);
        } else {
            (void)Ctrl_Pause(now);
        }
        return true;
    }
    return false;
}

void Ctrl_Rebase(uint32_t now)
{
    s_ledLast  = now;
    s_rampLast = now;
}

void Ctrl_GetInfo(CtrlInfo *out)
{
    out->mode         = s_mode;
    out->resumeTo     = s_resumeTo;
    out->freqHz       = Pwm_GetFreq();
    out->dutyApplied  = Pwm_GetDuty();
    out->dutyManual   = s_manual;
    out->rampDuty     = s_ramp;
    out->rampDir      = s_dir;
    out->ledOn        = s_ledOn;
    out->shortIgnored = s_shortIgnored;
}

const char *Ctrl_ModeName(CtrlMode m)
{
    switch (m) {
    case MODE_AUTO:  return "AUTO";
    case MODE_PAUSE: return "PAUSA";
    default:         return "MANUAL";
    }
}
