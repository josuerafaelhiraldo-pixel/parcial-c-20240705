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
/* ---- PWM / captura (TIM3 -> PA6 -> PA0 -> TIM2) ---- */
extern int sim_pwm_disconnected;      /* 1: sin puente PA6-PA0 (PA0 queda en 0 por el pull-down) */
extern int sim_period_error_ppm;      /* error simulado del reloj de captura, en ppm (0 = ideal) */
extern int sim_pwm_start_calls, sim_ug_calls, sim_ug_before_start;
extern int sim_unsafe_writes, sim_arr_writes, sim_ccr_writes, sim_oc_preload_enabled;
extern uint32_t sim_hclk, sim_pclk1;
extern GPIO_InitTypeDef sim_gpio_init_last;
extern int sim_gpio_init_calls;
void sim_inject_capture(uint32_t per, uint32_t hi);   /* captura arbitraria en TIM2 */
void sim_pwm_edge(void);              /* un periodo completo del PWM llega a TIM2 (flanco de subida) */
void sim_tim2_enter(void);            /* antes de ejecutar firmware: carga las banderas de TIM2 */
void sim_tim2_leave(void);            /* despues: aplica "escribir 0 borra" (rc_w0) */
void sim_pwm_run_us(uint32_t us);     /* avanza us microsegundos de PWM: genera los periodos que caben */
void sim_reset_pwm_model(void);
#endif
