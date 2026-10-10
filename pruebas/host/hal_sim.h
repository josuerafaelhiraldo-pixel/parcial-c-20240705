#ifndef HAL_SIM_H
#define HAL_SIM_H
#include "stm32f4xx_hal.h"
#include "tim.h"
extern uint8_t sim_tx_out[];
extern size_t  sim_tx_out_len;
extern int     sim_rx_arm_calls, sim_tx_calls;
bool sim_tx_complete(void);
void sim_tx_drain(void);
void sim_rx_byte(uint8_t b);
void sim_rx_error(uint32_t flags);
void sim_set_btn(int level);          /* nivel de PC13 (1 = alto) */
void sim_tick(uint32_t n);            /* n interrupciones de TIM6 (y n ms de SysTick) */
extern int sim_tim_start_calls;
extern uint32_t sim_systick_ms;
#endif
