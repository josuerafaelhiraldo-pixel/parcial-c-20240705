/* app.c - Hito 2: tick de 1 ms (TIM6), B1 por EXTI con antirrebote, consola.
 *
 * Aplicacion de prueba: eco por UART, control de LD2 y teclas de un solo
 * caracter. Las teclas y las acciones de B1 (corta = alternar LD2, larga =
 * apagar LD2) son TEMPORALES: en el Hito 4 se reemplazan por la maquina de
 * estados y el interprete de ordenes (HELP, STATUS, MODE, ...). */
#include "app.h"
#include "app_config.h"
#include "button.h"
#include "console.h"
#include "tick.h"
#include "stm32f4xx_hal.h"

#define APP_BYTES_PER_LOOP   32U    /* limite de trabajo por vuelta de main */
#define APP_WRAP_TEST_START  0xFFFFF000U   /* tecla w: faltan 4096 ms para el desbordamiento de uint32 */

static uint8_t  s_prev;             /* byte anterior (para tratar CRLF como uno solo) */
static uint32_t s_lastService;      /* ultimo tick de 1 ms atendido */
static uint32_t s_mTim, s_mSys;     /* referencias de la tecla m (TIM6 y SysTick) */

static void led_set(bool on)
{
    HAL_GPIO_WritePin(APP_LED_PORT, APP_LED_PIN,
                      on ? APP_LED_ON_LEVEL : APP_LED_OFF_LEVEL);
}

static int btn_level(void)
{
    return (HAL_GPIO_ReadPin(APP_BTN_PORT, APP_BTN_PIN) == GPIO_PIN_SET) ? 1 : 0;
}

/* Compara el tiempo de TIM6 con el de SysTick desde la ultima vez que se pulso m.
 * Ambos salen del mismo reloj HCLK: sirve para comprobar la cuenta de TIM6
 * (preescalador y periodo), no para medir la exactitud absoluta del reloj. */
static void print_tick(void)
{
    uint32_t t = Tick_Ms();
    uint32_t h = HAL_GetTick();
    uint32_t dt = (uint32_t)(t - s_mTim);
    uint32_t dh = (uint32_t)(h - s_mSys);

    s_mTim = t;
    s_mSys = h;
    (void)Console_Printf("Tick: TIM6=%lu ms (+%lu)  SysTick=%lu ms (+%lu)  diferencia=%ld ms\r\n",
                         (unsigned long)t, (unsigned long)dt,
                         (unsigned long)h, (unsigned long)dh,
                         (long)(int32_t)(dt - dh));
}

static void print_help(void)
{
    (void)Console_Write("Teclas de prueba (Hito 2):\r\n"
                        "  1 = LD2 encendido   0 = LD2 apagado   t = alternar LD2\r\n"
                        "  b = nivel de B1     s = estadisticas  m = comparar TIM6/SysTick\r\n"
                        "  w = prueba de desbordamiento del tiempo (UINT32_MAX en 4,1 s)\r\n"
                        "  h = esta ayuda\r\n"
                        "B1: corta = alternar LD2, larga (1,5 s) = apagar LD2\r\n");
}

static void print_stats(void)
{
    ConsoleStats st;
    ButtonStats  bs;

    Console_GetStats(&st);
    (void)Console_Printf("RX: bytes=%lu overflow_buffer=%lu\r\n",
                         (unsigned long)st.rxBytes, (unsigned long)st.rxRingOverflow);
    (void)Console_Printf("UART: ORE=%lu FE=%lu NE=%lu PE=%lu\r\n",
                         (unsigned long)st.uartOverrun, (unsigned long)st.uartFraming,
                         (unsigned long)st.uartNoise, (unsigned long)st.uartParity);
    (void)Console_Printf("TX: bytes=%lu mensajes_descartados=%lu\r\n",
                         (unsigned long)st.txBytes, (unsigned long)st.txDropped);

    Button_GetStats(&bs);
    (void)Console_Printf("B1: flancos_isr=%lu cortas=%lu largas=%lu ultima=%lu ms estado=%s\r\n",
                         (unsigned long)bs.edgesIsr, (unsigned long)bs.shortCount,
                         (unsigned long)bs.longCount, (unsigned long)bs.lastDurationMs,
                         (bs.pressed != 0U) ? "pulsado" : "suelto");
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
    case 'm':
        (void)Console_Write("\r\n");
        print_tick();
        break;
    case 'w':
        /* Prueba inyectada: el contador salta a 4096 ms antes de UINT32_MAX. Se
         * reinician las marcas de tiempo del modulo del boton y los contadores. */
        Tick_SetMs(APP_WRAP_TEST_START);
        s_lastService = Tick_Ms();
        Button_Init(s_lastService);
        s_mTim = Tick_Ms();
        s_mSys = HAL_GetTick();
        (void)Console_Printf("\r\nTick_Ms = %lu: UINT32_MAX en 4096 ms. Contadores de B1 en 0.\r\n",
                             (unsigned long)Tick_Ms());
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
    Tick_Init();
    s_lastService = Tick_Ms();
    Button_Init(s_lastService);
    s_mTim = Tick_Ms();
    s_mSys = HAL_GetTick();

    (void)Console_Write("\r\n=== Parcial 20240705 | NUCLEO-F446RE | Hito 2: tick TIM6 y B1 con antirrebote ===\r\n");
    (void)Console_Write("USART2 115200 8N1. Escribe h para ver las teclas de prueba.\r\n");
    (void)Console_Printf("B1 (PC13) en reposo: nivel %d\r\n", btn_level());
}

void App_Loop(void)
{
    uint8_t     c;
    uint32_t    n = 0U;
    uint32_t    now = Tick_Ms();
    ButtonEvent ev;
    ButtonStats bs;

    Console_Service();

    while ((n < APP_BYTES_PER_LOOP) && Console_ReadByte(&c)) {
        handle_byte(c);
        n++;
    }

    /* Una vez por cada tick de 1 ms: antirrebote y clasificacion de B1. */
    if ((uint32_t)(now - s_lastService) >= 1U) {
        s_lastService = now;
        ev = Button_Service(now);
        if (ev != BTN_EVT_NONE) {
            Button_GetStats(&bs);
            if (ev == BTN_EVT_SHORT) {
                HAL_GPIO_TogglePin(APP_LED_PORT, APP_LED_PIN);
                (void)Console_Printf("\r\n[B1] corta #%lu, duracion %lu ms\r\n",
                                     (unsigned long)bs.shortCount,
                                     (unsigned long)bs.lastDurationMs);
            } else {
                led_set(false);
                (void)Console_Printf("\r\n[B1] larga #%lu (1500 ms)\r\n",
                                     (unsigned long)bs.longCount);
            }
        }
    }
}
