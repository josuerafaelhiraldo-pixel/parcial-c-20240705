/* console.c - Consola por USART2, no bloqueante, con buffers circulares.
 *
 * Contextos:
 *   main : Console_Write*, Console_ReadByte, Console_Service
 *   ISR  : HAL_UART_RxCpltCallback, HAL_UART_TxCpltCallback,
 *          HAL_UART_ErrorCallback (llamados desde USART2_IRQHandler)
 *
 * RX: la ISR es productora de s_rx y main es consumidora.
 * TX: main es productora de s_tx y la ISR (fin de transmision) consumidora.
 */
#include "console.h"
#include "ringbuf.h"
#include "app_config.h"
#include "stm32f4xx_hal.h"
#include "usart.h"          /* huart2 (lo genera CubeMX) */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static uint8_t  s_rxStore[APP_RX_BUF_SIZE];
static uint8_t  s_txStore[APP_TX_BUF_SIZE];
static RingBuf  s_rx;
static RingBuf  s_tx;

static uint8_t           s_rxByte;      /* destino del HAL para cada byte recibido */
static volatile bool     s_txBusy;      /* hay una transmision en curso */
static volatile uint16_t s_txChunk;     /* bytes del tramo en curso */

/* Contadores: cada uno lo modifica un solo contexto. */
static volatile uint32_t s_rxBytes, s_rxRingOverflow;
static volatile uint32_t s_ore, s_fe, s_ne, s_pe;
static volatile uint32_t s_txBytes;
static uint32_t          s_txDropped;   /* solo main */

/* ---- seccion critica corta (guarda y restaura PRIMASK) ---- */
static inline uint32_t irq_save(void)
{
    uint32_t pm = __get_PRIMASK();
    __disable_irq();
    return pm;
}

static inline void irq_restore(uint32_t pm)
{
    __set_PRIMASK(pm);
}

/* Arranca la transmision del siguiente tramo contiguo, si no hay una en curso.
 * Se puede llamar desde main o desde la ISR. */
static void tx_kick(void)
{
    uint32_t pm = irq_save();

    if (!s_txBusy) {
        uint16_t n = RingBuf_Contiguous(&s_tx);
        if (n > 0U) {
            if (HAL_UART_Transmit_IT(&huart2, RingBuf_TailPtr(&s_tx), n) == HAL_OK) {
                s_txChunk = n;
                s_txBusy  = true;
            }
            /* si el HAL esta ocupado, Console_Service() reintenta */
        }
    }
    irq_restore(pm);
}

static void rx_arm(void)
{
    (void)HAL_UART_Receive_IT(&huart2, &s_rxByte, 1U);
}

void Console_Init(void)
{
    (void)RingBuf_Init(&s_rx, s_rxStore, APP_RX_BUF_SIZE);
    (void)RingBuf_Init(&s_tx, s_txStore, APP_TX_BUF_SIZE);
    s_txBusy  = false;
    s_txChunk = 0U;
    rx_arm();
}

void Console_Service(void)
{
    uint32_t pm = irq_save();
    /* Recuperacion: si la recepcion quedo detenida, se rearma. */
    if (huart2.RxState == HAL_UART_STATE_READY) {
        rx_arm();
    }
    irq_restore(pm);

    tx_kick();      /* reintenta si algun arranque anterior fallo */
}

bool Console_ReadByte(uint8_t *b)
{
    return RingBuf_Get(&s_rx, b);
}

bool Console_WriteBytes(const uint8_t *p, uint16_t n)
{
    uint16_t i;

    if (n == 0U) {
        return true;
    }
    /* Todo o nada: asi no quedan mensajes cortados a la mitad.
     * Free() solo puede aumentar mientras tanto (la ISR libera espacio). */
    if (RingBuf_Free(&s_tx) < n) {
        s_txDropped++;
        return false;
    }
    for (i = 0U; i < n; i++) {
        (void)RingBuf_Put(&s_tx, p[i]);
    }
    tx_kick();
    return true;
}

bool Console_Write(const char *s)
{
    return Console_WriteBytes((const uint8_t *)s, (uint16_t)strlen(s));
}

bool Console_Printf(const char *fmt, ...)
{
    char    tmp[APP_PRINTF_MAX];
    va_list ap;
    int     n;

    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);

    if (n < 0) {
        return false;
    }
    if ((size_t)n >= sizeof(tmp)) {
        n = (int)sizeof(tmp) - 1;       /* mensaje truncado */
    }
    return Console_WriteBytes((const uint8_t *)tmp, (uint16_t)n);
}

void Console_GetStats(ConsoleStats *out)
{
    out->rxBytes        = s_rxBytes;
    out->rxRingOverflow = s_rxRingOverflow;
    out->uartOverrun    = s_ore;
    out->uartFraming    = s_fe;
    out->uartNoise      = s_ne;
    out->uartParity     = s_pe;
    out->txBytes        = s_txBytes;
    out->txDropped      = s_txDropped;
}

/* ================= Callbacks del HAL (contexto de ISR) ================= */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != &huart2) {
        return;
    }
    s_rxBytes++;
    if (!RingBuf_Put(&s_rx, s_rxByte)) {
        s_rxRingOverflow++;             /* buffer lleno: se pierde el byte y se cuenta */
    }
    rx_arm();                           /* quedar listo para el siguiente byte */
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != &huart2) {
        return;
    }
    RingBuf_Skip(&s_tx, s_txChunk);     /* libera los bytes ya enviados */
    s_txBytes += s_txChunk;
    s_txBusy = false;
    tx_kick();                          /* si quedan datos, sigue con el siguiente tramo */
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    uint32_t e;

    if (huart != &huart2) {
        return;
    }
    e = HAL_UART_GetError(huart);
    if ((e & HAL_UART_ERROR_ORE) != 0U) { s_ore++; }
    if ((e & HAL_UART_ERROR_FE)  != 0U) { s_fe++;  }
    if ((e & HAL_UART_ERROR_NE)  != 0U) { s_ne++;  }
    if ((e & HAL_UART_ERROR_PE)  != 0U) { s_pe++;  }
    rx_arm();       /* tras un overrun el HAL detiene la recepcion: se rearma */
}
