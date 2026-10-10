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
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *g, uint16_t pin)
{
    if ((g == &sim_GPIOA) && (pin == GPIO_PIN_0)) {      /* PA0: lo que llega por el puente desde PA6 */
        return (!sim_pwm_disconnected && sim_TIM3.CCR1 >= sim_TIM3.ARR + 1U && sim_TIM3.CCR1 != 0U)
               ? GPIO_PIN_SET : GPIO_PIN_RESET;
    }
    return pin_state[g->id][pin_index(pin)];
}
void HAL_GPIO_TogglePin(GPIO_TypeDef *g, uint16_t pin)
{ int i = pin_index(pin); pin_state[g->id][i] = pin_state[g->id][i] ? GPIO_PIN_RESET : GPIO_PIN_SET; }

/* ---- TIM2 / TIM3 / TIM6 / SysTick ---- */
TIM_TypeDef sim_TIM2, sim_TIM3 = { .PSC = 83 }, sim_TIM6;
TIM_HandleTypeDef htim3 = { &sim_TIM3 }, htim6 = { &sim_TIM6 };
int sim_tim_flag_clears, sim_tim_start_calls;
int sim_unsafe_writes, sim_arr_writes, sim_ccr_writes, sim_oc_preload_enabled;
int sim_pwm_start_calls, sim_ug_calls, sim_ug_before_start;
int sim_pwm_disconnected, sim_period_error_ppm;
uint32_t sim_systick_ms;
uint32_t sim_hclk = 84000000U, sim_pclk1 = 42000000U;
GPIO_InitTypeDef sim_gpio_init_last;
int sim_gpio_init_calls;

uint32_t HAL_RCC_GetHCLKFreq(void)  { return sim_hclk; }
uint32_t HAL_RCC_GetPCLK1Freq(void) { return sim_pclk1; }
void HAL_GPIO_Init(GPIO_TypeDef *g, GPIO_InitTypeDef *init) { (void)g; sim_gpio_init_last = *init; sim_gpio_init_calls++; }

HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *h)
{
    if (h->Instance != TIM6) return HAL_ERROR;
    sim_tim_start_calls++;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *h, uint32_t ch)
{
    (void)ch;
    h->Instance->CR1 |= TIM_CR1_CEN;
    sim_pwm_start_calls++;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_GenerateEvent(TIM_HandleTypeDef *h, uint32_t ev)
{
    (void)ev;
    sim_ug_calls++;
    if ((h->Instance->CR1 & TIM_CR1_CEN) == 0U) sim_ug_before_start++;
    return HAL_OK;
}

/* Modelo de la captura: el firmware escribe los registros de TIM3 y, en el
 * "hardware", cada periodo del PWM produce una captura en TIM2 (CCR1 = periodo,
 * CCR2 = tiempo en alto, en cuentas del reloj comun). Los registros de TIM3 se
 * toman como efectivos desde que se escriben (la precarga se prueba aparte).
 * sim_period_error_ppm distorsiona lo medido, para comprobar que una frecuencia
 * mala se detecta. */
static uint32_t hw_sr;                                  /* banderas reales de TIM2 */

void sim_reset_pwm_model(void)
{
    hw_sr = 0; sim_pwm_disconnected = 0; sim_period_error_ppm = 0;
    sim_hclk = 84000000U; sim_pclk1 = 42000000U;
}

/* Inyecta una captura "a mano" (para probar capturas no validas). */
void sim_inject_capture(uint32_t per, uint32_t hi)
{
    sim_TIM2.CCR1 = per; sim_TIM2.CCR2 = hi;
    if (hw_sr & TIM_SR_CC1IF) hw_sr |= TIM_SR_CC1OF;
    hw_sr |= TIM_SR_CC1IF | TIM_SR_CC2IF;
}

void sim_tim2_enter(void) { sim_TIM2.SR = hw_sr; }
void sim_tim2_leave(void) { hw_sr &= sim_TIM2.SR; }

static int pwm_constant(void)                           /* 1: sin flancos */
{
    uint32_t arr = sim_TIM3.ARR, ccr = sim_TIM3.CCR1;
    return sim_pwm_disconnected || (ccr == 0U) || (ccr >= arr + 1U);
}

void sim_pwm_edge(void)
{
    uint64_t per, hi;
    if (pwm_constant()) return;
    per = ((uint64_t)sim_TIM3.ARR + 1U) * (sim_TIM3.PSC + 1U);
    hi  = (uint64_t)sim_TIM3.CCR1 * (sim_TIM3.PSC + 1U);
    per = per * (uint64_t)(1000000LL + sim_period_error_ppm) / 1000000U;
    hi  = hi  * (uint64_t)(1000000LL + sim_period_error_ppm) / 1000000U;
    sim_TIM2.CCR1 = (uint32_t)per;
    sim_TIM2.CCR2 = (uint32_t)hi;
    if (hw_sr & TIM_SR_CC1IF) hw_sr |= TIM_SR_CC1OF;    /* captura sin leer: sobrecaptura */
    hw_sr |= TIM_SR_CC1IF | TIM_SR_CC2IF;
}

/* Avanza us microsegundos: genera los flancos que caben (frecuencia de TIM3 con el reloj comun). */
void sim_pwm_run_us(uint32_t us)
{
    static uint64_t phase;                              /* en cuentas de reloj acumuladas */
    uint64_t per;
    if (pwm_constant() || sim_TIM3.ARR == 0U) { phase = 0; return; }
    per = ((uint64_t)sim_TIM3.ARR + 1U) * (sim_TIM3.PSC + 1U);
    phase += (uint64_t)us * ((sim_pclk1 == sim_hclk) ? sim_pclk1 : 2U * sim_pclk1) / 1000000U;
    while (phase >= per) { phase -= per; sim_pwm_edge(); }
}

uint32_t HAL_GetTick(void) { return sim_systick_ms; }

void sim_tick(uint32_t n)
{
    while (n--) {
        sim_systick_ms++;
        HAL_TIM_PeriodElapsedCallback(&htim6);
    }
}

/* Como en el HAL real, los callbacks son debiles: si un modulo no los define,
 * no pasa nada. Asi cada prueba solo enlaza los modulos que necesita. */
__attribute__((weak)) void HAL_UART_TxCpltCallback(UART_HandleTypeDef *h) { (void)h; }
__attribute__((weak)) void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h) { (void)h; }
__attribute__((weak)) void HAL_UART_ErrorCallback(UART_HandleTypeDef *h) { (void)h; }
__attribute__((weak)) void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *h) { (void)h; }
__attribute__((weak)) void HAL_GPIO_EXTI_Callback(uint16_t p) { (void)p; }
