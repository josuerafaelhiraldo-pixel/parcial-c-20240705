/* button.c - Pulsador B1: EXTI13 en ambos flancos + antirrebote de 30 ms + corta/larga. */
#include "button.h"
#include "app_config.h"
#include "stm32f4xx_hal.h"

/* ---- ISR: unico dato compartido. Escribe solo la ISR, lee main. ---- */
static volatile uint32_t s_isrEdges;

/* ---- Estado de main (nadie mas lo toca) ---- */
static uint32_t s_seenEdges;        /* ultimo valor de s_isrEdges atendido */
static uint8_t  s_candPressed;      /* nivel candidato (el ultimo nivel leido) */
static uint32_t s_candSince;        /* instante en que ese nivel empezo a ser estable */
static uint8_t  s_pressed;          /* estado VALIDADO: 1 = pulsado */
static uint32_t s_pressStart;       /* instante en que se valido la pulsacion */
static uint8_t  s_longFired;        /* la larga ya se emitio (o se inhibe la corta) */
static uint32_t s_shortCount, s_longCount, s_lastDur;

static uint8_t raw_pressed(void)
{
    return (HAL_GPIO_ReadPin(APP_BTN_PORT, APP_BTN_PIN) == APP_BTN_PRESSED_LEVEL) ? 1U : 0U;
}

/* ISR de EXTI13. El vector EXTI15_10 y la limpieza de la bandera los hace el HAL
 * (stm32f4xx_it.c -> HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13)). Aqui solo se anota
 * que hubo un flanco. Varios flancos seguidos (rebotes) solo suben el contador:
 * el contador NO es el numero de pulsaciones. */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == APP_BTN_PIN) {
        s_isrEdges++;
    }
}

void Button_Init(uint32_t nowMs)
{
    uint8_t raw = raw_pressed();

    s_seenEdges   = s_isrEdges;
    s_candPressed = raw;
    s_candSince   = nowMs;
    s_pressed     = raw;
    s_pressStart  = nowMs;
    /* Si B1 ya estaba pulsado al arrancar, esa pulsacion no genera eventos:
     * se marca como "larga ya emitida" hasta que se suelte. */
    s_longFired   = raw;
    s_shortCount = 0U;
    s_longCount  = 0U;
    s_lastDur    = 0U;
}

ButtonEvent Button_Service(uint32_t nowMs)
{
    ButtonEvent ev = BTN_EVT_NONE;
    uint32_t    edges = s_isrEdges;
    uint8_t     raw = raw_pressed();

    /* 1) Si la ISR vio algun flanco desde la ultima vez, el nivel no es estable:
     *    se reinicia la ventana, aunque el pin se vea igual (rebote entre ticks). */
    if (edges != s_seenEdges) {
        s_seenEdges = edges;
        s_candSince = nowMs;
    }
    /* 2) Si el nivel muestreado cambio respecto al candidato, se reinicia la ventana. */
    if (raw != s_candPressed) {
        s_candPressed = raw;
        s_candSince = nowMs;
    }

    /* 3) Aceptar el cambio tras APP_DEBOUNCE_MS de nivel estable (pulsar o soltar). */
    if ((s_candPressed != s_pressed) &&
        ((uint32_t)(nowMs - s_candSince) >= APP_DEBOUNCE_MS)) {
        s_pressed = s_candPressed;

        if (s_pressed != 0U) {                      /* pulsacion validada */
            s_pressStart = nowMs;
            s_longFired  = 0U;
        } else {                                    /* liberacion validada */
            uint32_t dur = (uint32_t)(nowMs - s_pressStart);

            s_lastDur = dur;
            if (s_longFired == 0U) {
                if (dur >= APP_LONG_PRESS_MS) {     /* alcanzo 1500 ms justo al soltar */
                    s_longFired = 1U;
                    s_longCount++;
                    ev = BTN_EVT_LONG;
                } else if (dur >= APP_SHORT_MIN_MS) {
                    s_shortCount++;
                    ev = BTN_EVT_SHORT;
                } else {
                    /* menor de 30 ms: no cuenta */
                }
            }
            /* si la larga ya salio, al soltar no se emite nada (ni corta) */
        }
    }

    /* 4) Larga: se emite una sola vez al llegar a 1500 ms de pulsacion validada. */
    if ((ev == BTN_EVT_NONE) && (s_pressed != 0U) && (s_longFired == 0U) &&
        ((uint32_t)(nowMs - s_pressStart) >= APP_LONG_PRESS_MS)) {
        s_longFired = 1U;
        s_longCount++;
        ev = BTN_EVT_LONG;
    }

    return ev;
}

void Button_GetStats(ButtonStats *out)
{
    out->edgesIsr       = s_isrEdges;
    out->shortCount     = s_shortCount;
    out->longCount      = s_longCount;
    out->lastDurationMs = s_lastDur;
    out->pressed        = s_pressed;
}
