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

/* PWM de hardware: TIM3 canal 1 en PA6 (activo en alto). */
#define APP_PWM_FREQ_DEFAULT_HZ     1000U   /* arranque: 1 kHz ... */
#define APP_PWM_DUTY_DEFAULT_PCT    25U     /* ... y 25 % */

/* Maquina de estados (MANUAL / AUTO / PAUSA). */
#define APP_RAMP_MIN_PCT        10U     /* AUTO: la rampa va de 10 % ... */
#define APP_RAMP_MAX_PCT        90U     /* ... a 90 % y vuelve */
#define APP_RAMP_STEP_MS        20U     /* 1 punto porcentual cada 20 ms (80 pasos = 1,6 s por tramo) */
#define APP_LED_AUTO_MS         500U    /* AUTO: LD2 alterna cada 500 ms */
#define APP_LED_PAUSE_MS        125U    /* PAUSA: LD2 alterna cada 125 ms */
#define APP_TELEMETRY_MS        1000U   /* STREAM ON: una linea TEL cada 1000 ms */

/* Interprete de ordenes. */
#define APP_LINE_MAX            63U     /* caracteres por linea (mas el NUL) */
#define APP_BYTES_PER_LOOP      32U     /* limite de bytes de RX atendidos por vuelta de main */
#define APP_CONSOLE_ECHO        1       /* 1: la placa devuelve (eco) lo que se escribe */

/* Diagnostico (no forma parte del protocolo del enunciado). 1 = existe la orden DIAG
 * (DIAG MEASURE, DIAG PWMALL, DIAG STALL n, DIAG WRAP); 0 = DIAG responde ERR COMMAND. */
#define APP_DIAG_ENABLED        1
#define APP_DIAG_STALL_MIN_MS   100U
#define APP_DIAG_STALL_MAX_MS   5000U
#define APP_WRAP_TEST_START     0xFFFFF800U   /* DIAG WRAP: faltan 2048 ms para el desbordamiento de uint32 */

/* Medidor interno del PWM: TIM2 en modo "entrada PWM" sobre PA0 (CH1).
 * Requiere un puente de PA6 a PA0 (Arduino A0). */
#define APP_METER_PORT              GPIOA
#define APP_METER_PIN               GPIO_PIN_0
#define APP_METER_WINDOW_MS         200U    /* duracion de cada medicion */
#define APP_METER_DISCARD           2U      /* capturas que se descartan al empezar */
#define APP_METER_MIN_SAMPLES       10U     /* periodos minimos para dar una medida por valida */
#define APP_METER_MAX_SAMPLES       1000U   /* tope de periodos acumulados */
#define APP_METER_FREQ_TOL_CENTI    200     /* error de frecuencia admitido: 2,00 % (en centesimas de %) */
#define APP_METER_DUTY_TOL_CENTI    200     /* error de duty admitido: 2,00 puntos (centesimas de punto) */
#define APP_CHECK_SETTLE_MS         20U     /* espera tras cambiar el PWM, antes de medir */

/* Buffers de la consola. Deben ser potencias de 2.
 * Un buffer de N bytes guarda como maximo N-1 (siempre queda un hueco).
 * RX: 256 -> 255 utiles (el reto pide al menos 128). */
#define APP_RX_BUF_SIZE     256U
#define APP_TX_BUF_SIZE     1024U

/* Tamano maximo de un mensaje formateado con Console_Printf. */
#define APP_PRINTF_MAX      256U

#endif /* APP_CONFIG_H */
