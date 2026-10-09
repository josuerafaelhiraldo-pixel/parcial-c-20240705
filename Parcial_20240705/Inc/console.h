/* console.h - Consola por USART2 (puerto serie virtual del ST-LINK).
 *
 *  - Recepcion por interrupcion, un byte a la vez, hacia un buffer circular.
 *  - Transmision por interrupcion desde una cola circular: las funciones de
 *    escritura NUNCA esperan; si no hay espacio, el mensaje completo se
 *    descarta y se cuenta (txDropped).
 *  - Esta consola es la unica duena de USART2: no usar otro driver sobre ella.
 */
#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t rxBytes;          /* bytes recibidos y aceptados por la ISR */
    uint32_t rxRingOverflow;   /* bytes perdidos porque el buffer RX estaba lleno */
    uint32_t uartOverrun;      /* errores ORE del hardware */
    uint32_t uartFraming;      /* errores de trama (FE) */
    uint32_t uartNoise;        /* errores de ruido (NE) */
    uint32_t uartParity;       /* errores de paridad (PE) */
    uint32_t txBytes;          /* bytes enviados */
    uint32_t txDropped;        /* mensajes descartados por cola TX llena */
} ConsoleStats;

void Console_Init(void);       /* tras MX_USART2_UART_Init() */
void Console_Service(void);    /* llamar en cada vuelta de main */

bool Console_ReadByte(uint8_t *b);                       /* no bloquea */
bool Console_WriteBytes(const uint8_t *p, uint16_t n);   /* no bloquea */
bool Console_Write(const char *s);                       /* no bloquea */
bool Console_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

void Console_GetStats(ConsoleStats *out);

#endif /* CONSOLE_H */
