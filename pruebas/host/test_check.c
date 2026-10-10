/* Pruebas en PC de pwm_check.c: medicion unica, autoprueba de 15 combinaciones,
 * deteccion de fallos, reintento con la cola TX llena y restauracion del PWM. */
#include "pwm.h"
#include "pwm_meter.h"
#include "pwm_check.h"
#include "console.h"
#include "tick.h"
#include "hal_sim.h"
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALLO linea %d: %s\n", __LINE__, #c); fails++; } } while (0)

static const char *out(void) { sim_tx_out[sim_tx_out_len] = 0; return (const char *)sim_tx_out; }
static void clear(void) { sim_tx_out_len = 0; }

/* Un milisegundo: tick de 1 ms y 10 vueltas de main de 100 us. */
static void ms1(int drain)
{
    sim_tick(1);
    for (int k = 0; k < 10; k++) {
        sim_pwm_run_us(100);
        sim_tim2_enter();
        Console_Service();
        PwmCheck_Service(Tick_Ms());
        sim_tim2_leave();
    }
    if (drain) sim_tx_drain();
}

static uint32_t run_until_idle(uint32_t limit_ms)
{
    uint32_t n = 0;
    while (PwmCheck_Busy() && n < limit_ms) { ms1(1); n++; }
    return n;
}

static int count_str(const char *hay, const char *needle)
{
    int n = 0;
    for (const char *p = hay; (p = strstr(p, needle)) != NULL; p += strlen(needle)) n++;
    return n;
}

int main(void)
{
    uint32_t n;

    sim_reset_pwm_model();
    Console_Init();
    Tick_Init();
    Pwm_Init();
    Meter_Init();
    CHECK(!PwmCheck_Busy());

    /* --- medicion unica de la salida actual (1 kHz, 25 %) --- */
    CHECK(PwmCheck_StartOne(Tick_Ms()));
    CHECK(PwmCheck_Busy());
    CHECK(!PwmCheck_StartOne(Tick_Ms()) && !PwmCheck_StartAll(Tick_Ms()));   /* ocupado: rechaza */
    n = run_until_idle(1000);
    CHECK(!PwmCheck_Busy());
    CHECK(n >= 215 && n <= 230);                 /* 20 ms de espera + 200 ms de ventana */
    CHECK(strstr(out(), "[medida] 1000 Hz duty  25 % -> f=1000.0 Hz (+0.00 %) duty=25.00 % (+0.00 pp)") != NULL);
    CHECK(strstr(out(), "OK\r\n") != NULL && strstr(out(), "FALLA") == NULL);
    CHECK(Pwm_GetFreq() == 1000U && Pwm_GetDuty() == 25U);                   /* no cambia lo programado */
    clear();

    /* --- medicion que cruza el desbordamiento del tiempo: dura lo mismo --- */
    Tick_SetMs(0xFFFFFFF0U);
    clear();
    CHECK(PwmCheck_StartOne(Tick_Ms()));
    n = run_until_idle(1000);
    CHECK(n >= 215 && n <= 230);
    CHECK(Tick_Ms() < 300U);                                                  /* el contador dio la vuelta */
    CHECK(strstr(out(), "OK\r\n") != NULL && strstr(out(), "FALLA") == NULL);
    clear();

    /* --- autoprueba completa, partiendo de 2000 Hz / 75 % --- */
    CHECK(Pwm_Set(2000U, 75U));
    CHECK(PwmCheck_StartAll(Tick_Ms()));
    n = run_until_idle(10000);
    CHECK(!PwmCheck_Busy());
    CHECK(n >= 3200 && n <= 3600);               /* 15 x (20 + 200) ms = 3300 ms */
    CHECK(strstr(out(), "Autoprueba PWM") != NULL);
    for (int i = 1; i <= 15; i++) {
        char tag[40];
        (void)snprintf(tag, sizeof tag, "[%2d/15]", i);
        CHECK(count_str(out(), tag) == 1);       /* cada linea, una sola vez */
    }
    CHECK(strstr(out(), "[ 1/15]  500 Hz duty   0 % -> sin flancos, nivel BAJO, capturas=0") != NULL);
    CHECK(strstr(out(), "[ 5/15]  500 Hz duty 100 % -> sin flancos, nivel ALTO, capturas=0") != NULL);
    CHECK(strstr(out(), "[ 8/15] 1000 Hz duty  50 % -> f=1000.0 Hz (+0.00 %) duty=50.00 % (+0.00 pp)") != NULL);
    CHECK(strstr(out(), "[14/15] 2000 Hz duty  75 % -> f=2000.0 Hz (+0.00 %) duty=75.00 % (+0.00 pp)") != NULL);
    CHECK(strstr(out(), "FALLA") == NULL);
    CHECK(strstr(out(), "Resultado: 15/15 OK  (reloj TIM=84000000 Hz, resolucion 11.9 ns, ventana 200 ms)") != NULL);
    CHECK(strstr(out(), "diferencia 0 ms") != NULL);       /* el tick de TIM6 no se altero */
    CHECK(strstr(out(), "no sustituye a un instrumento") != NULL);
    CHECK(Pwm_GetFreq() == 2000U && Pwm_GetDuty() == 75U); /* restaurado */
    clear();

    /* --- frecuencia con error de 3 %: los 9 casos con flancos fallan, los 6 de nivel pasan --- */
    sim_period_error_ppm = 30000;
    CHECK(PwmCheck_StartAll(Tick_Ms()));
    run_until_idle(10000);
    CHECK(count_str(out(), "FALLA") == 9);
    CHECK(strstr(out(), "Resultado: 6/15 OK") != NULL);
    CHECK(strstr(out(), "(-2.91 %)") != NULL);
    sim_period_error_ppm = 0;
    clear();

    /* --- puente suelto: solo pasan los tres de 0 % --- */
    sim_pwm_disconnected = 1;
    CHECK(PwmCheck_StartAll(Tick_Ms()));
    run_until_idle(10000);
    CHECK(strstr(out(), "Resultado: 3/15 OK") != NULL);
    CHECK(count_str(out(), "FALLA") == 12);
    sim_pwm_disconnected = 0;
    clear();

    /* --- cola TX llena: el resultado no se pierde, se reintenta --- */
    CHECK(PwmCheck_StartAll(Tick_Ms()));
    for (int i = 0; i < 2500; i++) ms1(0);        /* nadie vacia la cola TX durante 2,5 s */
    CHECK(PwmCheck_Busy());                        /* esperando espacio para imprimir */
    run_until_idle(10000);
    for (int i = 1; i <= 15; i++) {
        char tag[40];
        (void)snprintf(tag, sizeof tag, "[%2d/15]", i);
        CHECK(count_str(out(), tag) == 1);
    }
    CHECK(strstr(out(), "Resultado: 15/15 OK") != NULL);
    CHECK(Pwm_GetFreq() == 2000U && Pwm_GetDuty() == 75U);

    /* --- cola TX llena justo al empezar el resumen: ningun mensaje del resumen se pierde --- */
    clear();
    CHECK(PwmCheck_StartAll(Tick_Ms()));
    {
        int filled = 0, it = 0;
        while (PwmCheck_Busy() && it < 200000) {
            if (it % 10 == 0) sim_tick(1);
            sim_pwm_run_us(100);
            sim_tim2_enter(); Console_Service(); PwmCheck_Service(Tick_Ms()); sim_tim2_leave();
            it++;
            if (!filled) {
                sim_tx_drain();
                if (strstr(out(), "[15/15]") != NULL) {              /* acaba de salir la ultima linea */
                    while (Console_Write("0123456789012345678901234567890123456789012345678901234567\r\n")) { }
                    filled = 1;
                    clear();
                }
            } else if (it % 10000 == 0) {
                break;                                               /* sin vaciar la cola, no avanza */
            }
        }
        CHECK(filled);
        CHECK(PwmCheck_Busy());                                      /* esperando espacio */
        CHECK(strstr(out(), "Resultado") == NULL);
        for (int i = 0; i < 5 && PwmCheck_Busy(); i++) {              /* ahora si se vacia */
            sim_tx_drain();
            for (int k = 0; k < 10; k++) { sim_tim2_enter(); Console_Service(); PwmCheck_Service(Tick_Ms()); sim_tim2_leave(); }
            sim_tx_drain();
        }
        sim_tx_drain();
        CHECK(!PwmCheck_Busy());
        CHECK(count_str(out(), "Resultado: 15/15 OK") == 1);
        CHECK(count_str(out(), "Tick durante la autoprueba") == 1);
        CHECK(count_str(out(), "no sustituye a un instrumento") == 1);
        CHECK(strstr(out(), "Resultado") < strstr(out(), "Tick durante") && strstr(out(), "Tick durante") < strstr(out(), "no sustituye"));
    }

    printf(fails ? "VERIFICACION PWM: %d FALLOS\n" : "VERIFICACION PWM: todas las pruebas OK\n", fails);
    return fails != 0;
}
