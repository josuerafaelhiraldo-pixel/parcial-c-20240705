/* tick.c - Base de tiempo de 1 ms con TIM6 (interrupcion de actualizacion).
 * El manejador TIM6_DAC_IRQHandler lo genera CubeMX y llama a HAL_TIM_IRQHandler,
 * que a su vez llama a HAL_TIM_PeriodElapsedCallback (definida aqui). */
#include "tick.h"
#include "tim.h"                    /* htim6, generado por CubeMX */
#include "stm32f4xx_hal.h"

static volatile uint32_t s_ms;      /* escribe la ISR; main solo lo lee */

void Tick_Init(void)
{
    s_ms = 0U;
    /* La inicializacion de TIM6 genera un evento de actualizacion que deja la
     * bandera UIF levantada. Se borra para no contar un milisegundo de mas. */
    __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
    (void)HAL_TIM_Base_Start_IT(&htim6);
}

uint32_t Tick_Ms(void)
{
    return s_ms;                    /* lectura de 32 bits alineada: atomica en Cortex-M4 */
}

void Tick_SetMs(uint32_t ms)
{
    uint32_t pm = __get_PRIMASK();

    __disable_irq();                /* la ISR no puede incrementar a mitad de la escritura */
    s_ms = ms;
    __set_PRIMASK(pm);
}

/* ISR de TIM6 (1 kHz): solo cuenta. Nada de impresion, esperas ni logica. */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        s_ms++;
    }
}
