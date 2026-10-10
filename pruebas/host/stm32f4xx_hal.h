/* Simulacion minima del HAL de ST, SOLO para probar la logica en un PC.
 * No se compila en la placa. */
#ifndef STUB_STM32F4XX_HAL_H
#define STUB_STM32F4XX_HAL_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum { HAL_OK = 0, HAL_ERROR = 1, HAL_BUSY = 2, HAL_TIMEOUT = 3 } HAL_StatusTypeDef;
typedef enum { HAL_UART_STATE_RESET = 0, HAL_UART_STATE_READY = 0x20, HAL_UART_STATE_BUSY = 0x24 } HAL_UART_StateTypeDef;

typedef struct { int dummy; } USART_TypeDef;
typedef struct {
    USART_TypeDef             *Instance;
    volatile HAL_UART_StateTypeDef gState;
    volatile HAL_UART_StateTypeDef RxState;
    volatile uint32_t          ErrorCode;
} UART_HandleTypeDef;

#define HAL_UART_ERROR_NONE 0x00U
#define HAL_UART_ERROR_PE   0x01U
#define HAL_UART_ERROR_NE   0x02U
#define HAL_UART_ERROR_FE   0x04U
#define HAL_UART_ERROR_ORE  0x08U

HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *h, const uint8_t *p, uint16_t n);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *h, uint8_t *p, uint16_t n);
uint32_t          HAL_UART_GetError(UART_HandleTypeDef *h);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *h);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *h);

/* CMSIS */
uint32_t __get_PRIMASK(void);
void     __set_PRIMASK(uint32_t v);
void     __disable_irq(void);

/* GPIO */
typedef struct { int id; } GPIO_TypeDef;
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;
extern GPIO_TypeDef sim_GPIOA, sim_GPIOC;
#define GPIOA (&sim_GPIOA)
#define GPIOC (&sim_GPIOC)
#define GPIO_PIN_5  0x0020U
#define GPIO_PIN_13 0x2000U
void          HAL_GPIO_WritePin(GPIO_TypeDef *g, uint16_t pin, GPIO_PinState s);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *g, uint16_t pin);
void          HAL_GPIO_TogglePin(GPIO_TypeDef *g, uint16_t pin);

/* GPIO: inicializacion (solo lo que usa pwm_meter.c) */
#define GPIO_PIN_0  0x0001U
#define GPIO_MODE_AF_PP        0x02U
#define GPIO_NOPULL            0x00U
#define GPIO_PULLDOWN          0x02U
#define GPIO_SPEED_FREQ_LOW    0x00U
#define GPIO_AF1_TIM2          0x01U
typedef struct { uint32_t Pin, Mode, Pull, Speed, Alternate; } GPIO_InitTypeDef;
extern GPIO_InitTypeDef sim_gpio_init_last;
extern int sim_gpio_init_calls;
void HAL_GPIO_Init(GPIO_TypeDef *g, GPIO_InitTypeDef *init);
#define __HAL_RCC_GPIOA_CLK_ENABLE()  ((void)0)
#define __HAL_RCC_TIM2_CLK_ENABLE()   ((void)0)

/* RCC: relojes */
extern uint32_t sim_hclk, sim_pclk1;
uint32_t HAL_RCC_GetHCLKFreq(void);
uint32_t HAL_RCC_GetPCLK1Freq(void);

/* Registros: mismos nombres y bits que CMSIS (RM0390). */
#define SET_BIT(REG, BIT)    ((REG) |= (BIT))
#define CLEAR_BIT(REG, BIT)  ((REG) &= ~(BIT))
typedef struct {
    volatile uint32_t CR1, CR2, SMCR, DIER, SR, EGR, CCMR1, CCMR2, CCER, CNT, PSC, ARR, RCR,
                      CCR1, CCR2, CCR3, CCR4;
} TIM_TypeDef;
extern TIM_TypeDef sim_TIM2, sim_TIM3, sim_TIM6;
#define TIM2 (&sim_TIM2)
#define TIM3 (&sim_TIM3)
#define TIM6 (&sim_TIM6)
#define TIM_CR1_CEN   0x0001U
#define TIM_CR1_UDIS  0x0002U
#define TIM_CR1_ARPE  0x0080U
#define TIM_SMCR_SMS_2 0x0004U
#define TIM_SMCR_TS_0  0x0010U
#define TIM_SMCR_TS_2  0x0040U
#define TIM_SR_CC1IF  0x0002U
#define TIM_SR_CC2IF  0x0004U
#define TIM_SR_CC1OF  0x0200U
#define TIM_SR_CC2OF  0x0400U
#define TIM_EGR_UG    0x0001U
#define TIM_CCMR1_CC1S_0 0x0001U
#define TIM_CCMR1_CC2S_1 0x0200U
#define TIM_CCER_CC1E 0x0001U
#define TIM_CCER_CC2E 0x0010U
#define TIM_CCER_CC2P 0x0020U
#define TIM_CHANNEL_1 0x00000000U
#define TIM_EVENTSOURCE_UPDATE 0x0001U

typedef struct { TIM_TypeDef *Instance; } TIM_HandleTypeDef;
#define TIM_FLAG_UPDATE 0x01U
extern int sim_tim_flag_clears;
#define __HAL_TIM_CLEAR_FLAG(h, f) ((void)sim_tim_flag_clears++, (h)->Instance->SR = ~(uint32_t)(f))
/* Escrituras de ARR y CCR: se anotan las que ocurren con el contador en marcha
 * y las actualizaciones NO detenidas (UDIS = 0): seria un cambio no atomico. */
extern int sim_unsafe_writes, sim_arr_writes, sim_ccr_writes;
#define SIM_NOTE_WRITE(h) do { if ((((h)->Instance->CR1 & TIM_CR1_CEN) != 0U) && \
                                   (((h)->Instance->CR1 & TIM_CR1_UDIS) == 0U)) sim_unsafe_writes++; } while (0)
#define __HAL_TIM_SET_AUTORELOAD(h, v) do { SIM_NOTE_WRITE(h); sim_arr_writes++; (h)->Instance->ARR = (v); } while (0)
#define __HAL_TIM_GET_AUTORELOAD(h)    ((h)->Instance->ARR)
#define __HAL_TIM_SET_COMPARE(h, ch, v) do { SIM_NOTE_WRITE(h); sim_ccr_writes++; \
                                             (&(h)->Instance->CCR1)[(ch) >> 2U] = (v); } while (0)
#define __HAL_TIM_GET_COMPARE(h, ch)   ((&(h)->Instance->CCR1)[(ch) >> 2U])
extern int sim_oc_preload_enabled;
#define __HAL_TIM_ENABLE_OCxPRELOAD(h, ch) ((void)(h), (void)(ch), sim_oc_preload_enabled = 1)
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *h);
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *h, uint32_t ch);
HAL_StatusTypeDef HAL_TIM_GenerateEvent(TIM_HandleTypeDef *h, uint32_t ev);
void     HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *h);
uint32_t HAL_GetTick(void);
void     HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);
#endif
