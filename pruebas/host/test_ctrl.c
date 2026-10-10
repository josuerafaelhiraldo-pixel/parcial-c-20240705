/* Pruebas en PC de ctrl.c: MANUAL / AUTO / PAUSA, rampa 10..90 % (1 punto cada 20 ms),
 * patrones del LED, restauracion del duty manual, PAUSA sin recuperar pasos, accion de B1
 * y desbordamiento del contador de tiempo. Se llama a Ctrl_Service una vez por milisegundo. */
#include "ctrl.h"
#include "app_config.h"
#include "hal_sim.h"
#include "pwm.h"
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALLO linea %d: %s\n", __LINE__, #c); fails++; } } while (0)

static uint32_t now;
static int led_toggles;
static int last_led;
static uint32_t toggle_t[64];

static int led(void) { return HAL_GPIO_ReadPin(APP_LED_PORT, APP_LED_PIN) == APP_LED_ON_LEVEL; }
static void run(uint32_t n)
{
    while (n--) {
        now++;
        Ctrl_Service(now);
        if (led() != last_led) { if (led_toggles < 64) toggle_t[led_toggles] = now; led_toggles++; last_led = led(); }
    }
}
static void start(uint32_t t0)
{
    sim_reset_pwm_model();
    sim_TIM3.CR1 = 0U;                                        /* el contador no corre al llamar a Pwm_Init (reinicio del modelo) */
    Pwm_Init();
    now = t0;
    Ctrl_Init(now);
    led_toggles = 0; last_led = led();
}
#define RESET_TOG() do { led_toggles = 0; last_led = led(); } while (0)
static CtrlInfo info(void) { CtrlInfo i; Ctrl_GetInfo(&i); return i; }
/* duty esperado de la rampa tras k pasos (k = ms / 20), empezando en 10 y subiendo */
static uint32_t tri(uint32_t k)
{
    uint32_t pos = k % 160U;
    return (pos <= 80U) ? (10U + pos) : (10U + (160U - pos));
}

int main(void)
{
    CtrlInfo i;

    /* --- arranque --- */
    start(1000);
    i = info();
    CHECK(i.mode == MODE_MANUAL && i.freqHz == 1000U && i.dutyApplied == 25U && i.dutyManual == 25U);
    CHECK(led() == 1);
    run(5000);
    CHECK(led_toggles == 0);                                  /* MANUAL: LED fijo */
    CHECK(Pwm_GetDuty() == 25U);                              /* y sin rampa */
    CHECK(strcmp(Ctrl_ModeName(MODE_MANUAL), "MANUAL") == 0 && strcmp(Ctrl_ModeName(MODE_AUTO), "AUTO") == 0
          && strcmp(Ctrl_ModeName(MODE_PAUSE), "PAUSA") == 0);

    /* --- DUTY manual --- */
    CHECK(Ctrl_SetDuty(0) == CTRL_OK && Pwm_GetDuty() == 0U && info().dutyManual == 0U);
    CHECK(Ctrl_SetDuty(100) == CTRL_OK && Pwm_GetDuty() == 100U);
    CHECK(Ctrl_SetDuty(101) == CTRL_ERR_RANGE && Pwm_GetDuty() == 100U);   /* un rechazo no cambia nada */
    CHECK(Ctrl_SetDuty(60) == CTRL_OK && Pwm_GetDuty() == 60U && info().dutyManual == 60U);

    /* --- AUTO: empieza en 10 % ascendiendo; 2 ciclos completos exactos --- */
    CHECK(Ctrl_SetAuto(now) == CTRL_OK);
    CHECK(info().mode == MODE_AUTO && Pwm_GetDuty() == 10U && info().rampDir == 1);
    CHECK(info().dutyManual == 60U);                          /* el duty manual se conserva */
    {
        uint32_t t0 = now;
        for (uint32_t ms = 1; ms <= 6400U; ms++) {
            run(1);
            uint32_t want = tri(ms / 20U);
            if (Pwm_GetDuty() != want) { printf("  ms=%lu duty=%lu esperado=%lu\n", (unsigned long)ms, (unsigned long)Pwm_GetDuty(), (unsigned long)want); }
            CHECK(Pwm_GetDuty() == want);
            if (fails > 5) break;
        }
        (void)t0;
    }
    CHECK(Pwm_GetDuty() == 10U);                              /* 6400 ms = 2 ciclos de 3,2 s: de vuelta en 10 % */
    CHECK(sim_unsafe_writes == 0);
    /* LED en AUTO: alterna cada 500 ms */
    CHECK(led_toggles >= 12);
    for (int k = 0; k + 1 < led_toggles && k < 12; k++) {
        CHECK(toggle_t[k + 1] - toggle_t[k] == 500U);
    }
    /* tramo de subida = 1,6 s: 90 % exactamente a los 1600 ms de entrar */
    CHECK(Ctrl_SetAuto(now) == CTRL_OK);
    run(1599); CHECK(Pwm_GetDuty() == 89U && info().rampDir == 1);
    run(1);    CHECK(Pwm_GetDuty() == 90U && info().rampDir == -1);
    run(1599); CHECK(Pwm_GetDuty() == 11U);
    run(1);    CHECK(Pwm_GetDuty() == 10U && info().rampDir == 1);
    for (int k = 0; k < 20000; k++) { run(1); CHECK(Pwm_GetDuty() >= 10U && Pwm_GetDuty() <= 90U); }

    /* --- volver a MANUAL: restaura el ultimo duty manual y el LED queda fijo --- */
    CHECK(Ctrl_SetManual(now) == CTRL_OK);
    CHECK(info().mode == MODE_MANUAL && Pwm_GetDuty() == 60U && led() == 1);
    RESET_TOG(); run(3000); CHECK(led_toggles == 0 && Pwm_GetDuty() == 60U);
    CHECK(Ctrl_SetManual(now) == CTRL_OK && Pwm_GetDuty() == 60U);   /* MODE MANUAL en MANUAL: OK, sin cambios */

    /* --- DUTY en AUTO se rechaza y no cambia nada --- */
    CHECK(Ctrl_SetAuto(now) == CTRL_OK); run(100);
    { uint32_t d = Pwm_GetDuty(); CHECK(Ctrl_SetDuty(50) == CTRL_ERR_STATE); CHECK(Pwm_GetDuty() == d && info().dutyManual == 60U); }
    /* MODE AUTO estando en AUTO: vuelve a empezar desde 10 % ascendiendo */
    run(700); CHECK(Pwm_GetDuty() > 40U);
    CHECK(Ctrl_SetAuto(now) == CTRL_OK && Pwm_GetDuty() == 10U && info().rampDir == 1);
    run(19); CHECK(Pwm_GetDuty() == 10U);                     /* el primer paso llega a los 20 ms */
    run(1);  CHECK(Pwm_GetDuty() == 11U);

    /* --- FREQ: en MANUAL y AUTO, conserva el duty --- */
    CHECK(Ctrl_SetFreq(500) == CTRL_OK && Pwm_GetFreq() == 500U && Pwm_GetDuty() == 11U);
    CHECK(Ctrl_SetFreq(2000) == CTRL_OK && Pwm_GetFreq() == 2000U);
    CHECK(Ctrl_SetFreq(750) == CTRL_ERR_RANGE && Pwm_GetFreq() == 2000U);
    run(20); CHECK(Pwm_GetDuty() == 12U && Pwm_GetFreq() == 2000U);   /* la rampa sigue con la frecuencia nueva */
    CHECK(Ctrl_SetFreq(1000) == CTRL_OK);

    /* --- PAUSA desde AUTO subiendo --- */
    run(300);
    { uint32_t d = Pwm_GetDuty(); CtrlInfo b;
      CHECK(Ctrl_Pause(now) == CTRL_OK); b = info();
      CHECK(b.mode == MODE_PAUSE && b.resumeTo == MODE_AUTO && b.dutyApplied == 0U && Pwm_GetDuty() == 0U);
      CHECK(b.rampDuty == d && b.rampDir == 1 && b.dutyManual == 60U && b.freqHz == 1000U);
      RESET_TOG(); run(2000);
      CHECK(Pwm_GetDuty() == 0U && info().rampDuty == d);     /* pausado: la rampa no avanza */
      CHECK(led_toggles == 16);                               /* LED: alterna cada 125 ms (2000 / 125) */
      for (int k = 0; k + 1 < led_toggles; k++) CHECK(toggle_t[k + 1] - toggle_t[k] == 125U);
      /* en PAUSA se rechazan MODE, DUTY y FREQ */
      CHECK(Ctrl_SetManual(now) == CTRL_ERR_STATE && Ctrl_SetAuto(now) == CTRL_ERR_STATE);
      CHECK(Ctrl_SetDuty(40) == CTRL_ERR_STATE && Ctrl_SetFreq(500) == CTRL_ERR_STATE);
      CHECK(info().mode == MODE_PAUSE && info().freqHz == 1000U && info().dutyManual == 60U && info().rampDuty == d);
      /* PAUSE estando en PAUSA: OK y no cambia lo guardado (sigue volviendo a AUTO) */
      CHECK(Ctrl_Pause(now) == CTRL_OK && info().resumeTo == MODE_AUTO && info().mode == MODE_PAUSE);
      /* RESUME: continua desde el valor y direccion guardados, sin recuperar pasos */
      CHECK(Ctrl_Resume(now) == CTRL_OK);
      CHECK(info().mode == MODE_AUTO && Pwm_GetDuty() == d && info().rampDir == 1);
      run(19); CHECK(Pwm_GetDuty() == d);                     /* nada de pasos atrasados: espera 20 ms completos */
      run(1);  CHECK(Pwm_GetDuty() == d + 1U);
      CHECK(Ctrl_Resume(now) == CTRL_ERR_STATE);              /* fuera de PAUSA */
    }

    /* --- PAUSA desde AUTO bajando: la direccion se conserva --- */
    CHECK(Ctrl_SetAuto(now) == CTRL_OK);
    run(1600 + 20 * 30);                                      /* arriba en 90 y 30 pasos bajando: 60 % */
    CHECK(Pwm_GetDuty() == 60U && info().rampDir == -1);
    CHECK(Ctrl_Pause(now) == CTRL_OK); run(777);
    CHECK(Ctrl_Resume(now) == CTRL_OK);
    CHECK(Pwm_GetDuty() == 60U && info().rampDir == -1);
    run(20); CHECK(Pwm_GetDuty() == 59U);                     /* sigue bajando */
    run(20 * 49); CHECK(Pwm_GetDuty() == 10U && info().rampDir == 1);

    /* --- PAUSA desde MANUAL: RESUME vuelve a MANUAL con el duty manual --- */
    CHECK(Ctrl_SetManual(now) == CTRL_OK); CHECK(Ctrl_SetDuty(33) == CTRL_OK);
    CHECK(Ctrl_Pause(now) == CTRL_OK);
    CHECK(info().resumeTo == MODE_MANUAL && Pwm_GetDuty() == 0U && info().dutyManual == 33U);
    run(1000); CHECK(Pwm_GetDuty() == 0U);
    CHECK(Ctrl_Resume(now) == CTRL_OK && info().mode == MODE_MANUAL && Pwm_GetDuty() == 33U && led() == 1);
    RESET_TOG(); run(1000); CHECK(led_toggles == 0);
    CHECK(Ctrl_Resume(now) == CTRL_ERR_STATE);                /* RESUME fuera de PAUSA */

    /* --- B1: accion de cada pulsacion --- */
    CHECK(Ctrl_SetManual(now) == CTRL_OK);
    CHECK(Ctrl_OnButton(BTN_EVT_NONE, now) == false && info().mode == MODE_MANUAL);
    CHECK(Ctrl_OnButton(BTN_EVT_SHORT, now) == true && info().mode == MODE_AUTO && Pwm_GetDuty() == 10U);
    CHECK(Ctrl_OnButton(BTN_EVT_SHORT, now) == true && info().mode == MODE_MANUAL && Pwm_GetDuty() == 33U);
    CHECK(Ctrl_OnButton(BTN_EVT_LONG, now) == true && info().mode == MODE_PAUSE && info().resumeTo == MODE_MANUAL);
    CHECK(info().shortIgnored == 0U);
    CHECK(Ctrl_OnButton(BTN_EVT_SHORT, now) == false && info().mode == MODE_PAUSE && info().shortIgnored == 1U);   /* corta en PAUSA: se ignora */
    CHECK(Ctrl_OnButton(BTN_EVT_SHORT, now) == false && info().shortIgnored == 2U && Pwm_GetDuty() == 0U);
    CHECK(Ctrl_OnButton(BTN_EVT_LONG, now) == true && info().mode == MODE_MANUAL && Pwm_GetDuty() == 33U);
    CHECK(Ctrl_OnButton(BTN_EVT_SHORT, now) == true && info().mode == MODE_AUTO);
    run(500);
    CHECK(Ctrl_OnButton(BTN_EVT_LONG, now) == true && info().mode == MODE_PAUSE && info().resumeTo == MODE_AUTO);
    CHECK(Ctrl_OnButton(BTN_EVT_LONG, now) == true && info().mode == MODE_AUTO);

    /* --- el LED al entrar en cada estado empieza encendido; patrones desde ese instante --- */
    CHECK(Ctrl_SetManual(now) == CTRL_OK); run(333);
    CHECK(Ctrl_SetAuto(now) == CTRL_OK); CHECK(led() == 1);
    RESET_TOG(); run(499); CHECK(led_toggles == 0); run(1); CHECK(led_toggles == 1 && led() == 0);
    CHECK(Ctrl_Pause(now) == CTRL_OK); CHECK(led() == 1);
    RESET_TOG(); run(124); CHECK(led_toggles == 0); run(1); CHECK(led_toggles == 1 && led() == 0);

    /* --- retraso de main: los pasos atrasados se atienden de uno en uno, sin deriva --- */
    start(50000); CHECK(Ctrl_SetAuto(now) == CTRL_OK);
    now += 45;                                                /* main tardo 45 ms: vencieron los pasos de 20 y 40 ms */
    Ctrl_Service(now); CHECK(Pwm_GetDuty() == 11U);
    Ctrl_Service(now); CHECK(Pwm_GetDuty() == 12U);
    Ctrl_Service(now); CHECK(Pwm_GetDuty() == 12U);           /* ya no hay mas pasos vencidos */
    run(15); CHECK(Pwm_GetDuty() == 13U);                     /* el siguiente llega a los 60 ms: sin deriva */

    /* --- retraso de main y LED: la cuadricula de 500 ms se mantiene (sin deriva) --- */
    start(70000); CHECK(Ctrl_SetAuto(now) == CTRL_OK);
    now = 70000U + 510U; Ctrl_Service(now); CHECK(led() == 0);       /* el primer cambio llego tarde (510 ms) */
    now = 70000U + 999U; Ctrl_Service(now); CHECK(led() == 0);
    now = 70000U + 1000U; Ctrl_Service(now); CHECK(led() == 1);      /* el siguiente cae en los 1000 ms, no en 1010 */

    /* --- volver a AUTO despues de una rampa que bajaba: asciende desde 10 % --- */
    start(80000); CHECK(Ctrl_SetAuto(now) == CTRL_OK); run(2000);
    CHECK(info().rampDir == -1);
    CHECK(Ctrl_SetManual(now) == CTRL_OK); CHECK(Ctrl_SetAuto(now) == CTRL_OK);
    CHECK(info().rampDir == 1 && Pwm_GetDuty() == 10U);
    run(20); CHECK(Pwm_GetDuty() == 11U);

    /* --- desbordamiento del contador de tiempo: la rampa y el LED siguen --- */
    start(0xFFFFFF00U); CHECK(Ctrl_SetAuto(now) == CTRL_OK);
    for (uint32_t ms = 1; ms <= 3300U; ms++) {
        run(1);
        CHECK(Pwm_GetDuty() == tri(ms / 20U));
        if (fails > 5) break;
    }
    CHECK(now < 4000U);                                       /* el contador dio la vuelta a mitad de la prueba */
    CHECK(Pwm_GetDuty() == 10U + 0U || Pwm_GetDuty() == tri(3300U / 20U));
    for (int k = 0; k + 1 < led_toggles && k < 6; k++) CHECK(toggle_t[k + 1] - toggle_t[k] == 500U);
    CHECK(Ctrl_Pause(now) == CTRL_OK);
    start(0xFFFFFFF0U); CHECK(Ctrl_SetAuto(now) == CTRL_OK); CHECK(Ctrl_Pause(now) == CTRL_OK);
    RESET_TOG(); run(1000); CHECK(led_toggles == 8);      /* LED de PAUSA a caballo del desbordamiento */
    CHECK(Ctrl_Resume(now) == CTRL_OK);
    run(40); CHECK(Pwm_GetDuty() == 12U);

    /* --- Ctrl_Rebase: el tiempo salta de golpe sin que haya una rafaga de pasos --- */
    start(1000); CHECK(Ctrl_SetAuto(now) == CTRL_OK); run(100);
    { uint32_t d = Pwm_GetDuty();
      now = 0xFFFFF800U; Ctrl_Rebase(now);
      Ctrl_Service(now); Ctrl_Service(now); Ctrl_Service(now);
      CHECK(Pwm_GetDuty() == d);                              /* sin pasos fantasma */
      run(20); CHECK(Pwm_GetDuty() == d + 1U); }

    CHECK(sim_unsafe_writes == 0);
    printf(fails ? "CTRL: %d FALLOS\n" : "CTRL: todas las pruebas OK\n", fails);
    return fails != 0;
}
