/* button.h - Pulsador B1 (PC13): interrupcion EXTI, antirrebote y clasificacion.
 *
 * ISR (HAL_GPIO_EXTI_Callback): solo cuenta flancos. No lee tiempo, no imprime.
 * main (Button_Service, una vez por tick de 1 ms):
 *   - muestrea el pin y reinicia la ventana de 30 ms si el nivel cambia o si la
 *     ISR vio algun flanco;
 *   - acepta un cambio de estado solo tras 30 ms de nivel estable (pulsar y soltar);
 *   - corta: duracion validada de 30 ms a menos de 1500 ms, se emite al soltar;
 *   - larga: se emite UNA vez al llegar a 1500 ms; al soltar no genera corta.
 * Todas las diferencias de tiempo son (uint32_t)(ahora - antes). */
#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>

typedef enum {
    BTN_EVT_NONE = 0,
    BTN_EVT_SHORT,
    BTN_EVT_LONG
} ButtonEvent;

typedef struct {
    uint32_t edgesIsr;          /* flancos vistos por la ISR (con rebotes) */
    uint32_t shortCount;        /* pulsaciones cortas aceptadas */
    uint32_t longCount;         /* pulsaciones largas aceptadas */
    uint32_t lastDurationMs;    /* duracion validada de la ultima pulsacion terminada */
    uint8_t  pressed;           /* estado validado actual: 1 = pulsado */
} ButtonStats;

void        Button_Init(uint32_t nowMs);
ButtonEvent Button_Service(uint32_t nowMs);   /* llamar una vez por tick; max. 1 evento */
void        Button_GetStats(ButtonStats *out);

#endif /* BUTTON_H */
