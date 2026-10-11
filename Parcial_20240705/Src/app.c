/* app.c - Controlador de iluminacion (Hito 4): integra tick (TIM6), B1 (EXTI13), PWM (TIM3),
 * consola (USART2), maquina de estados (ctrl) e interprete de ordenes (shell).
 *
 * main no espera nada: en cada vuelta atiende la consola y el interprete, y una vez por cada
 * tick de 1 ms atiende el pulsador. Las ISR (EXTI13, TIM6, USART2) solo anotan datos. */
#include "app.h"
#include "app_config.h"
#include "button.h"
#include "console.h"
#include "ctrl.h"
#include "pwm.h"
#include "pwm_check.h"
#include "pwm_meter.h"
#include "shell.h"
#include "tick.h"
#include "stm32f4xx_hal.h"

static uint32_t s_lastService;      /* ultimo tick de 1 ms atendido (pulsador) */

static void print_boot(void)
{
    PwmInfo pi;

    (void)Console_Write("\r\n=== Parcial 20240705 | NUCLEO-F446RE | Controlador de iluminacion (Hito 4) ===\r\n");
    (void)Console_Write("USART2 115200 8N1. Escribe HELP para ver las ordenes.\r\n");
    Pwm_GetInfo(&pi);
    (void)Console_Printf("PWM: %lu Hz, duty %lu %%  (reloj TIM3=%lu Hz, PSC=%lu, ARR=%lu, CCR=%lu)\r\n",
                         (unsigned long)pi.freqHz, (unsigned long)pi.dutyPct,
                         (unsigned long)pi.timerClkHz, (unsigned long)pi.prescaler,
                         (unsigned long)pi.arr, (unsigned long)pi.ccr);
    (void)Console_Write("Modo MANUAL, STREAM OFF.\r\n");
}

void App_Init(void)
{
    uint32_t now;

    HAL_GPIO_WritePin(APP_LED_PORT, APP_LED_PIN, APP_LED_OFF_LEVEL);
    Console_Init();
    Tick_Init();
    now = Tick_Ms();
    s_lastService = now;
    Button_Init(now);
    Pwm_Init();
    Meter_Init();
    Ctrl_Init(now);
    Shell_Init(now);
    print_boot();
}

void App_Loop(void)
{
    uint32_t    now;
    ButtonEvent ev;
    bool        changed;

    Console_Service();
    Shell_Service(Tick_Ms());
    now = Tick_Ms();                /* se lee de nuevo: DIAG WRAP puede haber cambiado el tiempo */

    PwmCheck_Service(now);          /* en cada vuelta: los flancos del medidor se leen por sondeo */
    Ctrl_Service(now);              /* rampa de AUTO y patron del LED */

    /* Una vez por cada tick de 1 ms: antirrebote y clasificacion de B1. */
    if ((uint32_t)(now - s_lastService) >= 1U) {
        s_lastService = now;
        ev = Button_Service(now);
        if (ev != BTN_EVT_NONE) {
            changed = false;
            if (!PwmCheck_Busy()) {         /* durante una verificacion del PWM, B1 no cambia el estado */
                changed = Ctrl_OnButton(ev, now);
            }
            Shell_ButtonEvent(ev, changed);
        }
    }
}
