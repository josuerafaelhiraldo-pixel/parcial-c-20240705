/* pwm_check.h - Verificacion del PWM con el medidor interno, sin esperas.
 *
 *  PwmCheck_StartOne(): mide la salida actual y la compara con lo programado.
 *  PwmCheck_StartAll(): recorre 500/1000/2000 Hz x duty 0/25/50/75/100 %
 *                       (15 combinaciones), mide cada una, imprime una linea con
 *                       el veredicto y al final restaura la configuracion previa.
 *
 * Es una maquina de estados: PwmCheck_Service() se llama en CADA vuelta de main
 * (los flancos se leen por sondeo). Las lineas se imprimen con Console_Printf; si
 * la cola de transmision esta llena, se reintenta en la vuelta siguiente. */
#ifndef PWM_CHECK_H
#define PWM_CHECK_H

#include <stdint.h>
#include <stdbool.h>

#define PWM_CHECK_COMBOS   15U

bool PwmCheck_StartOne(uint32_t now);   /* false si ya hay una verificacion en curso */
bool PwmCheck_StartAll(uint32_t now);
bool PwmCheck_Busy(void);
void PwmCheck_Service(uint32_t now);

#endif /* PWM_CHECK_H */
