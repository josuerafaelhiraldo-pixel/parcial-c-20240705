/* pwm.h - Salida PWM por hardware: TIM3 canal 1 en PA6 (activo en alto).
 *
 *  - Frecuencias permitidas: 500, 1000 y 2000 Hz. Duty: entero de 0 a 100 %.
 *  - El periodo se calcula con el reloj REAL del temporizador (RCC) y el
 *    preescalador que tenga TIM3, no con una frecuencia de CPU supuesta:
 *        cuentas por periodo = (reloj_TIM3 / (PSC + 1)) / frecuencia
 *        ARR = cuentas - 1          CCR = duty * cuentas / 100
 *  - 0 %  -> CCR = 0        : la salida queda en nivel bajo constante.
 *    100 % -> CCR = ARR + 1 : CCR mayor que ARR, la salida queda en nivel alto
 *             constante. En ambos casos el hardware no genera ningun pulso.
 *  - ARR y CCR usan registros de precarga y se escriben con las actualizaciones
 *    detenidas (UDIS): el cambio entra completo al final del periodo en curso,
 *    sin periodos mezclados.
 *  - Solo usa TIM3. El tick de 1 ms (TIM6) no se toca al cambiar la frecuencia. */
#ifndef PWM_H
#define PWM_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t timerClkHz;   /* reloj real que recibe TIM3 */
    uint32_t prescaler;    /* registro PSC */
    uint32_t arr;          /* registro ARR (periodo - 1) */
    uint32_t ccr;          /* registro CCR1 */
    uint32_t freqHz;       /* frecuencia pedida */
    uint32_t dutyPct;      /* duty pedido */
} PwmInfo;

void     Pwm_Init(void);                              /* tras MX_TIM3_Init(): 1 kHz, 25 %, en marcha */
bool     Pwm_FreqIsValid(uint32_t hz);                /* 500, 1000 o 2000 */
bool     Pwm_SetFreq(uint32_t hz);                    /* conserva el duty */
bool     Pwm_SetDuty(uint32_t pct);                   /* conserva la frecuencia */
bool     Pwm_Set(uint32_t hz, uint32_t pct);          /* ambos a la vez; si algo es invalido no cambia nada */
uint32_t Pwm_GetFreq(void);
uint32_t Pwm_GetDuty(void);
void     Pwm_GetInfo(PwmInfo *out);
uint32_t Pwm_TimerClockHz(void);                      /* reloj real de los temporizadores del bus APB1 */

#endif /* PWM_H */
