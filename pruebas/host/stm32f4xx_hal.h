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
#endif
