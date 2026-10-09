/* Simulador del HAL para pruebas en PC. Imita el comportamiento relevante:
 *  - Transmit_IT devuelve BUSY si ya hay una transmision (como el HAL real).
 *  - Los bytes de TX se "leen" al completar (sim_tx_complete), no al llamar:
 *    asi se detecta si la consola sobrescribe datos que aun estan en vuelo.
 *  - El callback de fin de TX se invoca con gState ya en READY, como hace el HAL. */
#include "hal_sim.h"
#include <string.h>

UART_HandleTypeDef huart2 = { 0, HAL_UART_STATE_READY, HAL_UART_STATE_READY, 0 };
GPIO_TypeDef sim_GPIOA = { 1 }, sim_GPIOC = { 2 };

static const uint8_t *tx_ptr;
static uint16_t       tx_len;
static uint8_t       *rx_ptr;

uint8_t sim_tx_out[1 << 20];
size_t  sim_tx_out_len;
int     sim_rx_arm_calls, sim_tx_calls;

static uint32_t primask;
uint32_t __get_PRIMASK(void) { return primask; }
void     __set_PRIMASK(uint32_t v) { primask = v; }
void     __disable_irq(void) { primask = 1; }

HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *h, const uint8_t *p, uint16_t n)
{
    if (h->gState != HAL_UART_STATE_READY) return HAL_BUSY;
    if (n == 0U) return HAL_ERROR;
    tx_ptr = p; tx_len = n; h->gState = HAL_UART_STATE_BUSY; sim_tx_calls++;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *h, uint8_t *p, uint16_t n)
{
    (void)n;
    if (h->RxState != HAL_UART_STATE_READY) return HAL_BUSY;
    rx_ptr = p; h->RxState = HAL_UART_STATE_BUSY; h->ErrorCode = 0; sim_rx_arm_calls++;
    return HAL_OK;
}

uint32_t HAL_UART_GetError(UART_HandleTypeDef *h) { return h->ErrorCode; }

/* Termina la transmision en curso (si la hay). Devuelve true si habia una. */
bool sim_tx_complete(void)
{
    if (huart2.gState != HAL_UART_STATE_BUSY) return false;
    memcpy(&sim_tx_out[sim_tx_out_len], tx_ptr, tx_len);
    sim_tx_out_len += tx_len;
    huart2.gState = HAL_UART_STATE_READY;
    HAL_UART_TxCpltCallback(&huart2);
    return true;
}

void sim_tx_drain(void) { while (sim_tx_complete()) { } }

/* Entrega un byte recibido (la ISR del HAL lo deja en *rx_ptr y llama al callback). */
void sim_rx_byte(uint8_t b)
{
    if (huart2.RxState != HAL_UART_STATE_BUSY) return;   /* recepcion detenida: el byte se pierde */
    *rx_ptr = b;
    huart2.RxState = HAL_UART_STATE_READY;
    HAL_UART_RxCpltCallback(&huart2);
}

/* Error de hardware. ORE detiene la recepcion (como el HAL real); los demas no. */
void sim_rx_error(uint32_t flags)
{
    huart2.ErrorCode = flags;
    if (flags & HAL_UART_ERROR_ORE) huart2.RxState = HAL_UART_STATE_READY;
    HAL_UART_ErrorCallback(&huart2);
    huart2.ErrorCode = 0;
}

static GPIO_PinState pin_state[3][16];
void sim_set_btn(int level) { pin_state[2][13] = level ? GPIO_PIN_SET : GPIO_PIN_RESET; }

static int pin_index(uint16_t pin) { int i = 0; while (((pin >> i) & 1U) == 0U) i++; return i; }
void HAL_GPIO_WritePin(GPIO_TypeDef *g, uint16_t pin, GPIO_PinState s) { pin_state[g->id][pin_index(pin)] = s; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *g, uint16_t pin) { return pin_state[g->id][pin_index(pin)]; }
void HAL_GPIO_TogglePin(GPIO_TypeDef *g, uint16_t pin)
{ int i = pin_index(pin); pin_state[g->id][i] = pin_state[g->id][i] ? GPIO_PIN_RESET : GPIO_PIN_SET; }
