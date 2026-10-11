/* Pruebas de integracion en PC (app.c): B1 + maquina de estados + consola + PWM + telemetria juntos.
 * Reproduce las pruebas de aceptacion del enunciado que se pueden simular: AUTO durante 3,2 s,
 * 20 pulsaciones cortas, mantener B1 3 s, PAUSA desde AUTO y RESUME, STREAM ON con 10 ordenes/s
 * durante 30 s, patrones del LED, tiempos de respuesta y desbordamiento del contador de tiempo. */
#include "app.h"
#include "app_config.h"
#include "button.h"
#include "console.h"
#include "ctrl.h"
#include "hal_sim.h"
#include "pwm.h"
#include "shell.h"
#include "tick.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALLO linea %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void feed(const char *s) { while (*s) sim_rx_byte((uint8_t)*s++); }
static const char *out(void) { sim_tx_out[sim_tx_out_len] = 0; return (const char *)sim_tx_out; }
static void clear(void) { sim_tx_out_len = 0; }

static int led_toggles; static int last_led; static uint32_t toggle_t[256];
static int led(void) { return HAL_GPIO_ReadPin(APP_LED_PORT, APP_LED_PIN) == APP_LED_ON_LEVEL; }
static void reset_led(void) { led_toggles = 0; last_led = led(); }

static void ms(uint32_t n)
{
    while (n--) {
        sim_tick(1);
        for (int k = 0; k < 10; k++) {
            sim_pwm_run_us(100);
            sim_tim2_enter(); App_Loop(); sim_tim2_leave();
        }
        sim_tx_drain();
        if (led() != last_led) { if (led_toggles < 256) toggle_t[led_toggles] = Tick_Ms(); led_toggles++; last_led = led(); }
    }
}
static void btn(int pressed) { sim_set_btn(pressed ? 0 : 1); HAL_GPIO_EXTI_Callback(GPIO_PIN_13); }
/* pulsacion limpia: pulsa, mantiene hold ms, suelta, y deja pasar gap ms */
static void press(uint32_t hold, uint32_t gap) { btn(1); ms(hold); btn(0); ms(gap); }
static CtrlInfo info(void) { CtrlInfo i; Ctrl_GetInfo(&i); return i; }
static int count(const char *needle)
{
    int n = 0; const char *p = out();
    while ((p = strstr(p, needle)) != NULL) { n++; p += strlen(needle); }
    return n;
}
static void cmd(const char *line) { clear(); feed(line); feed("\r"); ms(10); }
static uint32_t tri(uint32_t k) { uint32_t pos = k % 160U; return (pos <= 80U) ? (10U + pos) : (10U + (160U - pos)); }
static ButtonStats bst(void) { ButtonStats b; Button_GetStats(&b); return b; }

int main(void)
{
    sim_reset_pwm_model();
    sim_set_btn(1);
    App_Init(); sim_tx_drain(); clear();
    reset_led();

    /* --- Encendido: MANUAL, 1000 Hz, 25 %, STREAM OFF, LED fijo --- */
    ms(3000);
    CHECK(info().mode == MODE_MANUAL && Pwm_GetFreq() == 1000U && Pwm_GetDuty() == 25U);
    CHECK(sim_tx_out_len == 0);                                 /* STREAM OFF: no sale nada solo */
    CHECK(led() == 1 && led_toggles == 0);

    /* --- pulsacion corta: MANUAL -> AUTO; el cambio se refleja dentro de 50 ms tras validarse --- */
    {
        btn(1); ms(100);
        CHECK(info().mode == MODE_MANUAL);                      /* se ejecuta al soltar */
        btn(0);
        uint32_t t = 0;
        while (info().mode != MODE_AUTO && t < 200U) { ms(1); t++; }
        CHECK(t >= 30U && t <= 31U);                            /* 30 ms de nivel estable al soltar; nada mas de retraso */
        CHECK(Pwm_GetDuty() == 10U && info().rampDir == 1);
        ms(100);
    }

    /* --- AUTO durante 3,2 s: sube a 90 %, baja y regresa a 10 % --- */
    cmd("MODE MANUAL"); cmd("MODE AUTO");
    {
        uint32_t d_min = 100, d_max = 0, jumps = 0, prev = Pwm_GetDuty();
        reset_led();
        for (uint32_t t = 1; t <= 3200U; t++) {
            ms(1);
            uint32_t d = Pwm_GetDuty();
            if (d < d_min) { d_min = d; }
            if (d > d_max) { d_max = d; }
            if ((d > prev ? d - prev : prev - d) > 1U) jumps++;
            prev = d;
            if (t == 1600U) CHECK(d == 90U);                    /* cada tramo dura 1,6 s */
        }
        CHECK(d_min == 10U && d_max == 90U && jumps == 0);
        CHECK(Pwm_GetDuty() == 10U);                            /* ciclo completo 3,2 s */
        CHECK(led_toggles == 6);                                /* LED: 500 ms por cambio */
    }
    cmd("MODE MANUAL"); CHECK(Pwm_GetDuty() == 25U);

    /* --- 20 pulsaciones cortas separadas: 20 eventos, sin duplicados (con rebotes) --- */
    {
        ButtonStats b0 = bst();
        for (int i = 0; i < 20; i++) {
            btn(1); ms(1); btn(0); ms(2); btn(1); ms(120); btn(0); ms(1); btn(1); ms(1); btn(0);    /* rebotes al pulsar y al soltar */
            ms(250);
        }
        ButtonStats b1 = bst();
        CHECK(b1.shortCount - b0.shortCount == 20U && b1.longCount == b0.longCount);
        CHECK(info().mode == MODE_MANUAL);                      /* 20 cambios: MANUAL -> AUTO -> ... -> MANUAL */
    }

    /* --- mantener B1 3 s: una larga (PAUSA); al soltar no hay corta --- */
    {
        ButtonStats b0 = bst();
        btn(1); ms(1499); CHECK(info().mode == MODE_MANUAL);
        ms(40); CHECK(info().mode == MODE_PAUSE);               /* la larga sale al llegar a 1500 ms (mas el antirrebote) */
        ms(1500);                                               /* mantiene hasta los 3 s: sin repeticiones */
        CHECK(info().mode == MODE_PAUSE && bst().longCount - b0.longCount == 1U);
        btn(0); ms(200);
        ButtonStats b1 = bst();
        CHECK(b1.longCount - b0.longCount == 1U && b1.shortCount == b0.shortCount);
        CHECK(info().mode == MODE_PAUSE && info().shortIgnored == 0U);
    }
    /* corta en PAUSA: se ignora */
    { press(100, 100); CHECK(info().mode == MODE_PAUSE && info().shortIgnored == 1U && Pwm_GetDuty() == 0U); }
    /* otra larga: reanuda al estado previo (MANUAL, 25 %) */
    { btn(1); ms(1600); btn(0); ms(100); CHECK(info().mode == MODE_MANUAL && Pwm_GetDuty() == 25U && led() == 1); }

    /* --- PAUSA desde AUTO (con B1 larga) y RESUME (con la orden): duty 0 y continua desde lo guardado --- */
    cmd("MODE AUTO"); ms(2000);                                 /* 100 pasos: subio a 90 y esta bajando */
    {
        CHECK(info().rampDir == -1 && Pwm_GetDuty() == tri(100U));
        btn(1); ms(1540); CHECK(info().mode == MODE_PAUSE && Pwm_GetDuty() == 0U);
        btn(0); ms(100);
        uint32_t saved = info().rampDuty; int savedDir = info().rampDir;
        CHECK(info().resumeTo == MODE_AUTO && saved >= 10U && saved <= 90U);
        reset_led(); ms(1000);
        CHECK(Pwm_GetDuty() == 0U && info().rampDuty == saved && info().rampDir == savedDir);   /* sin rampa durante la pausa */
        CHECK(led_toggles == 8);                                     /* LED cada 125 ms */
        cmd("RESUME");
        CHECK(info().mode == MODE_AUTO && Pwm_GetDuty() == saved && info().rampDir == savedDir);
        ms(10); CHECK(Pwm_GetDuty() == saved);                       /* sin pasos del tiempo pausado (19 ms desde RESUME) */
        ms(1);  CHECK((int)Pwm_GetDuty() == (int)saved + savedDir);  /* el primer paso llega a los 20 ms, en la misma direccion */
    }
    /* y desde AUTO bajando, con la pausa pedida por la orden */
    cmd("MODE AUTO"); ms(1600 + 20 * 25);
    {
        CHECK(Pwm_GetDuty() == 65U && info().rampDir == -1);
        cmd("PAUSE"); uint32_t saved = info().rampDuty; ms(2500);
        CHECK(Pwm_GetDuty() == 0U && info().rampDuty == saved && info().rampDir == -1);
        cmd("RESUME"); CHECK(Pwm_GetDuty() == saved);
        ms(30); CHECK(Pwm_GetDuty() == saved - 1U);
    }
    cmd("MODE MANUAL");

    /* --- STREAM ON y eventos de B1: aviso EVT solo con STREAM ON --- */
    clear(); press(100, 100); CHECK(count("EVT") == 0); press(100, 100);          /* STREAM OFF: silencio */
    cmd("STREAM ON"); clear();
    press(100, 100);
    CHECK(strstr(out(), "EVT B1 short ok mode=AUTO duty=") != NULL);
    clear(); btn(1); ms(1600); btn(0); ms(100);
    CHECK(strstr(out(), "EVT B1 long ok mode=PAUSA duty=0 ") != NULL);
    clear(); press(100, 100);
    CHECK(strstr(out(), "EVT B1 short ignored mode=PAUSA duty=0 ") != NULL);
    cmd("RESUME"); cmd("MODE MANUAL"); cmd("STREAM OFF");

    /* --- carga: STREAM ON y 10 ordenes por segundo durante 30 s, con la rampa y el boton activos --- */
    {
        ConsoleStats c0, c1; Console_GetStats(&c0);
        ShellStats s0; Shell_GetStats(&s0);
        cmd("MODE AUTO"); cmd("STREAM ON"); clear();
        uint32_t tAuto = Tick_Ms();                             /* la rampa se reinicio al recibir MODE AUTO */
        (void)tAuto;
        int bad_ramp = 0; uint32_t prev = Pwm_GetDuty();
        ButtonStats b0 = bst();
        for (int t = 0; t < 30000; t++) {
            if (t % 100 == 0) {
                static const char *q[3] = { "STATUS\r", "STATUS\n", "STATUS\r\n" };      /* los tres terminadores */
                feed(q[(t / 100) % 3]);                                                   /* 10 ordenes por segundo */
            }
            if (t == 12000) btn(1);                                                       /* una pulsacion en mitad de la carga */
            if (t == 12100) btn(0);
            ms(1);
            uint32_t d = Pwm_GetDuty();
            if ((info().mode == MODE_AUTO) && ((d > prev ? d - prev : prev - d) > 1U)) bad_ramp++;
            prev = d;
        }
        Console_GetStats(&c1);
        ShellStats s1; Shell_GetStats(&s1);
        CHECK(count("OK mode=") == 300);                                                  /* ninguna respuesta perdida */
        CHECK(count("ERR") == 0);
        CHECK(count("TEL up=") >= 29 && count("TEL up=") <= 30);
        CHECK(c1.txDropped == c0.txDropped && c1.rxRingOverflow == c0.rxRingOverflow);
        CHECK(c1.uartOverrun == c0.uartOverrun);
        CHECK(s1.cmdOk - s0.cmdOk == 300U + 2U + 0U);                                     /* 300 STATUS + MODE AUTO + STREAM ON */
        CHECK(bst().shortCount - b0.shortCount == 1U);                                    /* el boton respondio */
        CHECK(count("EVT B1 short ok") == 1);
        CHECK(bad_ramp == 0);                                                              /* la rampa nunca salto */
    }
    cmd("STREAM OFF"); cmd("MODE MANUAL");

    /* --- respuestas y eventos dentro de los limites de tiempo (en el simulador, 1 ms de resolucion) --- */
    {
        clear(); feed("STATUS"); sim_rx_byte('\r');
        uint32_t t = 0;
        while (strstr(out(), "OK mode=") == NULL && t < 100U) { ms(1); t++; }
        CHECK(t <= 2U);                                                                    /* <= 100 ms exigidos */
    }

    /* --- B1 y comandos al mismo tiempo: ninguno estorba al otro --- */
    cmd("MODE MANUAL"); cmd("DUTY 40");
    clear(); btn(1); feed("STATUS\r"); ms(60); btn(0); feed("DUTY 50\r"); ms(100);
    CHECK(count("OK mode=MANUAL") == 1 && count("OK DUTY 50") == 1);
    CHECK(info().mode == MODE_AUTO && info().dutyManual == 50U);
    cmd("MODE MANUAL");

    /* --- desbordamiento del contador de tiempo: DIAG WRAP, y luego B1 + rampa + telemetria --- */
    cmd("MODE AUTO"); cmd("STREAM ON"); ms(777);
    cmd("DIAG WRAP");
    {
        ButtonStats b0 = bst(); clear();
        ms(500);
        press(100, 100);                                        /* corta antes del desbordamiento */
        CHECK(bst().shortCount - b0.shortCount == 1U && info().mode == MODE_MANUAL);
        ms(1000);
        CHECK(Tick_Ms() > 0xFFFFF800U);
        press(100, 100);                                        /* AUTO otra vez */
        btn(1); ms(3000);                                       /* larga: empieza antes del desbordamiento y termina despues */
        CHECK(Tick_Ms() < 3000U);                               /* el contador ya dio la vuelta */
        CHECK(info().mode == MODE_PAUSE);
        btn(0); ms(100);
        CHECK(bst().longCount == b0.longCount + 1U && bst().shortCount - b0.shortCount == 2U);
        press(100, 100);                                        /* corta ignorada en PAUSA */
        CHECK(info().mode == MODE_PAUSE);
        btn(1); ms(1600); btn(0); ms(100);                      /* larga: reanuda */
        CHECK(info().mode == MODE_AUTO);
        reset_led(); ms(2000); CHECK(led_toggles == 4);         /* LED de AUTO tras el desbordamiento: 500 ms */
        uint32_t prev = Pwm_GetDuty(); int jumps = 0;
        for (int k = 0; k < 4000; k++) { ms(1); uint32_t d = Pwm_GetDuty(); if ((d > prev ? d - prev : prev - d) > 1U) jumps++; prev = d; }
        CHECK(jumps == 0);
        CHECK(count("TEL up=") >= 8);
    }
    cmd("STREAM OFF"); cmd("MODE MANUAL");

    /* --- durante una verificacion del PWM, B1 no cambia el estado (queda anotado como ignorado) --- */
    cmd("STREAM ON"); cmd("DIAG PWMALL"); clear();
    press(100, 100);
    CHECK(info().mode == MODE_MANUAL && strstr(out(), "EVT B1 short ignored mode=MANUAL") != NULL);
    ms(4000);
    CHECK(strstr(out(), "Resultado: 15/15 OK") != NULL && info().mode == MODE_MANUAL);
    press(100, 100); CHECK(info().mode == MODE_AUTO);              /* ya funciona otra vez */
    cmd("STREAM OFF"); cmd("MODE MANUAL");

    CHECK(sim_unsafe_writes == 0);
    printf(fails ? "APP: %d FALLOS\n" : "APP: todas las pruebas OK\n", fails);
    return fails != 0;
}
