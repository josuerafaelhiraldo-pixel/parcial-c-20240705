/* app_config.h - Configuracion propia de la aplicacion (NUCLEO-F446RE).
 * Alias de pines y tamanos. No depende de las etiquetas de CubeMX. */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* LED de estado LD2: PA5, activo en alto (UM1724: HIGH = encendido). */
#define APP_LED_PORT        GPIOA
#define APP_LED_PIN         GPIO_PIN_5
#define APP_LED_ON_LEVEL    GPIO_PIN_SET
#define APP_LED_OFF_LEVEL   GPIO_PIN_RESET

/* Pulsador B1 USER: PC13. Interrupcion EXTI13 en ambos flancos.
 * Activo en bajo: reposo = 1, pulsado = 0 (medido en el Hito 1). */
#define APP_BTN_PORT            GPIOC
#define APP_BTN_PIN             GPIO_PIN_13
#define APP_BTN_PRESSED_LEVEL   GPIO_PIN_RESET

/* Tiempos del pulsador (ms). Se miden sobre el estado ya validado. */
#define APP_DEBOUNCE_MS     30U     /* nivel estable necesario para aceptar un cambio */
#define APP_SHORT_MIN_MS    30U     /* corta: desde 30 ms ... */
#define APP_LONG_PRESS_MS   1500U   /* ... hasta menos de 1500 ms; la larga se dispara al llegar a 1500 */

/* Buffers de la consola. Deben ser potencias de 2.
 * Un buffer de N bytes guarda como maximo N-1 (siempre queda un hueco).
 * RX: 256 -> 255 utiles (el reto pide al menos 128). */
#define APP_RX_BUF_SIZE     256U
#define APP_TX_BUF_SIZE     512U

/* Tamano maximo de un mensaje formateado con Console_Printf. */
#define APP_PRINTF_MAX      128U

#endif /* APP_CONFIG_H */
