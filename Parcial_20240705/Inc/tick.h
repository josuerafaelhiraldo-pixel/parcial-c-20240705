/* tick.h - Base de tiempo de 1 ms con interrupcion de un temporizador (TIM6).
 *
 * TIM6 cuenta a 1 MHz (84 MHz / 84) y desborda cada 1000 cuentas = 1 ms.
 * La ISR solo incrementa un contador de 32 bits. SysTick queda para uso
 * interno del HAL y NO se usa como base de tiempo de la aplicacion.
 *
 * El contador da la vuelta a los 2^32 ms (unos 49,7 dias). Para medir tiempo
 * transcurrido use SIEMPRE la resta sin signo:  (uint32_t)(ahora - antes) >= limite
 * Nunca compare dos instantes con  ahora >= antes + limite. */
#ifndef TICK_H
#define TICK_H

#include <stdint.h>

void     Tick_Init(void);           /* pone el contador en 0 y arranca TIM6 con interrupcion */
uint32_t Tick_Ms(void);             /* milisegundos desde Tick_Init (da la vuelta en 2^32) */
void     Tick_SetMs(uint32_t ms);   /* SOLO para la prueba de desbordamiento (UINT32_MAX) */

#endif /* TICK_H */
