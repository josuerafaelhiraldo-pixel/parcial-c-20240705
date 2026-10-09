/* app.c - Hito 1: GPIO y consola.
 *
 * Aplicacion de prueba: eco por UART, control de LD2 y lectura de B1 con
 * teclas de un solo caracter. Estas teclas son TEMPORALES: en el Hito 4 se
 * reemplazan por el interprete de ordenes (HELP, STATUS, MODE, ...).
 * La lectura de B1 por sondeo tambien es temporal: en el Hito 2 pasa a EXTI. */
#include "app.h"
#include "app_config.h"
#include "console.h"
#include "stm32f4xx_hal.h"

#define APP_BYTES_PER_LOOP   32U    /* limite de trabajo por vuelta de main */

static uint8_t s_prev;              /* byte anterior (para tratar CRLF como uno solo) */
static int     s_btnLast;           /* ultimo nivel leido de PC13 */

static void led_set(bool on)
{
    HAL_GPIO_WritePin(APP_LED_PORT, APP_LED_PIN,
                      on ? APP_LED_ON_LEVEL : APP_LED_OFF_LEVEL);
}

static int btn_level(void)
{
    return (HAL_GPIO_ReadPin(APP_BTN_PORT, APP_BTN_PIN) == GPIO_PIN_SET) ? 1 : 0;
}

static void print_help(void)
{
    (void)Console_Write("Teclas de prueba (Hito 1):\r\n"
                        "  1 = LD2 encendido   0 = LD2 apagado   t = alternar LD2\r\n"
                        "  b = nivel de B1     s = estadisticas  h = esta ayuda\r\n");
}

static void print_stats(void)
{
    ConsoleStats st;

    Console_GetStats(&st);
    (void)Console_Printf("RX: bytes=%lu overflow_buffer=%lu\r\n",
                         (unsigned long)st.rxBytes, (unsigned long)st.rxRingOverflow);
    (void)Console_Printf("UART: ORE=%lu FE=%lu NE=%lu PE=%lu\r\n",
                         (unsigned long)st.uartOverrun, (unsigned long)st.uartFraming,
                         (unsigned long)st.uartNoise, (unsigned long)st.uartParity);
    (void)Console_Printf("TX: bytes=%lu mensajes_descartados=%lu\r\n",
                         (unsigned long)st.txBytes, (unsigned long)st.txDropped);
}

static void handle_byte(uint8_t c)
{
    if ((c == (uint8_t)'\n') && (s_prev == (uint8_t)'\r')) {   /* CRLF = un solo salto */
        s_prev = c;
        return;
    }
    s_prev = c;

    if ((c == (uint8_t)'\r') || (c == (uint8_t)'\n')) {
        (void)Console_Write("\r\n");
        return;
    }
    if ((c < 32U) || (c > 126U)) {
        return;                                   /* otros caracteres de control: se ignoran */
    }

    (void)Console_WriteBytes(&c, 1U);             /* eco */

    switch (c) {
    case '1':
        led_set(true);
        (void)Console_Write("\r\nLD2 encendido (PA5 en alto)\r\n");
        break;
    case '0':
        led_set(false);
        (void)Console_Write("\r\nLD2 apagado (PA5 en bajo)\r\n");
        break;
    case 't':
        HAL_GPIO_TogglePin(APP_LED_PORT, APP_LED_PIN);
        (void)Console_Write("\r\nLD2 alternado\r\n");
        break;
    case 'b':
        (void)Console_Printf("\r\nB1 (PC13) nivel = %d\r\n", btn_level());
        break;
    case 's':
        (void)Console_Write("\r\n");
        print_stats();
        break;
    case 'h':
    case '?':
        (void)Console_Write("\r\n");
        print_help();
        break;
    default:
        break;                                    /* solo eco */
    }
}

void App_Init(void)
{
    led_set(false);
    Console_Init();
    s_btnLast = btn_level();

    (void)Console_Write("\r\n=== Parcial 20240705 | NUCLEO-F446RE | Hito 1: GPIO y consola ===\r\n");
    (void)Console_Write("USART2 115200 8N1. Escribe h para ver las teclas de prueba.\r\n");
    (void)Console_Printf("B1 (PC13) en reposo: nivel %d\r\n", s_btnLast);
}

void App_Loop(void)
{
    uint8_t  c;
    uint32_t n = 0U;
    int      lvl;

    Console_Service();

    while ((n < APP_BYTES_PER_LOOP) && Console_ReadByte(&c)) {
        handle_byte(c);
        n++;
    }

    /* Sondeo temporal de B1: avisa cuando cambia el nivel (puede rebotar). */
    lvl = btn_level();
    if (lvl != s_btnLast) {
        s_btnLast = lvl;
        (void)Console_Printf("\r\n[B1] PC13 = %d (%s)\r\n", lvl, (lvl != 0) ? "alto" : "bajo");
    }
}
