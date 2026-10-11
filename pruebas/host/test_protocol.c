/* Pruebas en PC del protocolo de ordenes (shell + line + cmd + ctrl) usando App_Init / App_Loop
 * con el simulador de HAL: respuestas exactas, errores, CR/LF/CRLF, linea larga, perdida de bytes
 * en RX, telemetria, contrapresion de la cola TX y las ordenes de diagnostico. */
#include "app.h"
#include "app_config.h"
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
/* n ms de funcionamiento: tick de TIM6 y 10 vueltas de main por ms; la "ISR de TX" termina cada ms */
static void ms(uint32_t n)
{
    while (n--) {
        sim_tick(1);
        for (int k = 0; k < 10; k++) {
            sim_pwm_run_us(100);
            sim_tim2_enter(); App_Loop(); sim_tim2_leave();
        }
        sim_tx_drain();
    }
}
/* Respuesta sin el eco: lo que hay despues del primer CRLF */
static const char *resp(void) { const char *p = strstr(out(), "\r\n"); return p ? p + 2 : ""; }
/* Escribe una linea (con CR) y deja pasar 10 ms */
static void cmd(const char *line) { clear(); feed(line); feed("\r"); ms(10); }
static int count(const char *needle)
{
    int n = 0; const char *p = out();
    while ((p = strstr(p, needle)) != NULL) { n++; p += strlen(needle); }
    return n;
}
static CtrlInfo info(void) { CtrlInfo i; Ctrl_GetInfo(&i); return i; }
static ShellStats sh(void) { ShellStats s; Shell_GetStats(&s); return s; }
static int led(void) { return HAL_GPIO_ReadPin(APP_LED_PORT, APP_LED_PIN) == APP_LED_ON_LEVEL; }

int main(void)
{
    sim_reset_pwm_model();
    sim_set_btn(1);
    sim_rx_error(HAL_UART_ERROR_FE);                 /* un error de RX antes de arrancar el interprete: no debe costar la primera orden */
    App_Init(); sim_tx_drain();

    /* --- arranque: MANUAL, 1000 Hz, 25 %, STREAM OFF --- */
    CHECK(strstr(out(), "Hito 4") != NULL);
    CHECK(strstr(out(), "PWM: 1000 Hz, duty 25 %  (reloj TIM3=84000000 Hz, PSC=83, ARR=999, CCR=250)") != NULL);
    CHECK(strstr(out(), "Modo MANUAL, STREAM OFF.") != NULL);
    CHECK(info().mode == MODE_MANUAL && Pwm_GetFreq() == 1000U && Pwm_GetDuty() == 25U && !sh().stream);
    CHECK(led() == 1);
    CHECK(sim_pwm_start_calls == 1);
    clear();

    /* --- STATUS: campos y valores iniciales; eco; el uptime sale del tick de TIM6 --- */
    cmd("STATUS");
    CHECK(strncmp(out(), "STATUS\r\n", 8) == 0);                          /* eco de lo escrito + salto de linea */
    CHECK(strncmp(resp(), "OK mode=MANUAL freq=1000 duty=25 manual=25 ramp=10 dir=up resume=- uptime_ms=", 77) == 0);
    CHECK(strstr(resp(), " stream=OFF short=0 long=0 short_ign=0 cmd_ok=0 err_cmd=0 err_arg=0 err_range=0 err_state=0 err_ovf=0 rx_lost=0 uart_err=1 tx_drop=0\r\n") != NULL);
    CHECK(strlen(resp()) < 300);                                           /* cabe en RESP_MAX */
    {   unsigned long up = 0; const char *p = strstr(resp(), "uptime_ms=");
        CHECK(p != NULL); if (p) up = strtoul(p + 10, NULL, 10);
        CHECK(up == Tick_Ms() || up + 1 == Tick_Ms() || up + 2 == Tick_Ms() || up + 3 == Tick_Ms() || up + 10 >= Tick_Ms()); }

    /* --- HELP --- */
    cmd("HELP");
    {   const char *h = resp();
        CHECK(strstr(h, "HELP") && strstr(h, "STATUS") && strstr(h, "MODE MANUAL") && strstr(h, "MODE AUTO")
              && strstr(h, "DUTY n") && strstr(h, "FREQ n") && strstr(h, "PAUSE") && strstr(h, "RESUME")
              && strstr(h, "STREAM ON | OFF"));
        size_t l = strlen(h); CHECK(l > 200 && strcmp(h + l - 4, "OK\r\n") == 0);   /* termina en OK */ }

    cmd("STATUS"); CHECK(strstr(resp(), " cmd_ok=2 ") != NULL);            /* STATUS y HELP ya contaron como ordenes validas */

    /* --- respuestas exactas de las ordenes validas --- */
    cmd("DUTY 50");  CHECK(strcmp(resp(), "OK DUTY 50\r\n") == 0 && Pwm_GetDuty() == 50U && info().dutyManual == 50U);
    cmd("DUTY 0");   CHECK(strcmp(resp(), "OK DUTY 0\r\n") == 0 && Pwm_GetDuty() == 0U);
    cmd("DUTY 100"); CHECK(strcmp(resp(), "OK DUTY 100\r\n") == 0 && Pwm_GetDuty() == 100U);
    cmd("DUTY 25");  CHECK(strcmp(resp(), "OK DUTY 25\r\n") == 0 && Pwm_GetDuty() == 25U);
    cmd("FREQ 500"); CHECK(strcmp(resp(), "OK FREQ 500\r\n") == 0 && Pwm_GetFreq() == 500U && Pwm_GetDuty() == 25U);
    cmd("FREQ 2000"); CHECK(strcmp(resp(), "OK FREQ 2000\r\n") == 0 && Pwm_GetFreq() == 2000U);
    cmd("FREQ 1000"); CHECK(strcmp(resp(), "OK FREQ 1000\r\n") == 0 && Pwm_GetFreq() == 1000U);
    cmd("MODE MANUAL"); CHECK(strcmp(resp(), "OK MODE MANUAL duty=25\r\n") == 0);
    cmd("  DUTY   75  "); CHECK(strcmp(resp(), "OK DUTY 75\r\n") == 0 && Pwm_GetDuty() == 75U);
    cmd("DUTY 25");
    CHECK(sim_unsafe_writes == 0);

    /* --- errores: ninguna orden invalida cambia parametros --- */
    {
        static const struct { const char *l; const char *e; } bad[] = {
            { "FOO", "ERR COMMAND" }, { "status", "ERR COMMAND" }, { "duty 50", "ERR COMMAND" }, { "STATUSX", "ERR COMMAND" },
            { "DUTY 20abc", "ERR ARG" }, { "DUTY", "ERR ARG" }, { "DUTY 10 20", "ERR ARG" }, { "DUTY 5.5", "ERR ARG" },
            { "DUTY -1", "ERR RANGE" }, { "DUTY 101", "ERR RANGE" }, { "DUTY 99999999999999999999", "ERR RANGE" },
            { "DUTY 4294967396", "ERR RANGE" },
            { "FREQ 750", "ERR RANGE" }, { "FREQ 0", "ERR RANGE" }, { "FREQ 1000Hz", "ERR ARG" }, { "FREQ", "ERR ARG" },
            { "MODE", "ERR ARG" }, { "MODE FOO", "ERR ARG" }, { "STREAM", "ERR ARG" }, { "STREAM on", "ERR ARG" },
            { "HELP me", "ERR ARG" }, { "STATUS 1", "ERR ARG" }, { "PAUSE x", "ERR ARG" },
            { "RESUME", "ERR STATE" },
        };
        for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            char want[40]; snprintf(want, sizeof want, "%s\r\n", bad[i].e);
            cmd(bad[i].l);
            if (strcmp(resp(), want) != 0) printf("  '%s' -> '%s' (esperado '%s')\n", bad[i].l, resp(), bad[i].e);
            CHECK(strcmp(resp(), want) == 0);
            CHECK(Pwm_GetDuty() == 25U && Pwm_GetFreq() == 1000U && info().mode == MODE_MANUAL && info().dutyManual == 25U);
        }
        ShellStats s = sh();
        CHECK(s.errCommand == 4 && s.errArg == 13 && s.errRange == 6 && s.errState == 1 && s.errOverflow == 0);
    }
    cmd("STATUS");
    CHECK(strstr(resp(), " err_cmd=4 err_arg=13 err_range=6 err_state=1 err_ovf=0 ") != NULL);

    /* --- AUTO: DUTY se rechaza con ERR STATE; FREQ si se acepta --- */
    cmd("MODE AUTO"); CHECK(strcmp(resp(), "OK MODE AUTO duty=10\r\n") == 0 && info().mode == MODE_AUTO && Pwm_GetDuty() == 10U);
    cmd("DUTY 50");   CHECK(strcmp(resp(), "ERR STATE\r\n") == 0 && info().dutyManual == 25U);
    cmd("DUTY 200");  CHECK(strcmp(resp(), "ERR RANGE\r\n") == 0);                /* se valida antes de mirar el estado */
    cmd("FREQ 2000"); CHECK(strcmp(resp(), "OK FREQ 2000\r\n") == 0 && Pwm_GetFreq() == 2000U);
    cmd("FREQ 1000");
    cmd("MODE MANUAL"); CHECK(strcmp(resp(), "OK MODE MANUAL duty=25\r\n") == 0 && Pwm_GetDuty() == 25U);

    /* --- PAUSA --- */
    cmd("MODE AUTO"); ms(700);
    {
        uint32_t d = Pwm_GetDuty();
        cmd("PAUSE"); CHECK(strcmp(resp(), "OK PAUSE\r\n") == 0 && info().mode == MODE_PAUSE && Pwm_GetDuty() == 0U);
        CHECK(info().rampDuty >= d && info().resumeTo == MODE_AUTO);
        cmd("PAUSE"); CHECK(strcmp(resp(), "OK PAUSE\r\n") == 0 && info().resumeTo == MODE_AUTO);   /* ya pausado: OK, sin cambios */
        cmd("MODE MANUAL"); CHECK(strcmp(resp(), "ERR STATE\r\n") == 0);
        cmd("MODE AUTO");   CHECK(strcmp(resp(), "ERR STATE\r\n") == 0);
        cmd("DUTY 40");     CHECK(strcmp(resp(), "ERR STATE\r\n") == 0);
        cmd("FREQ 500");    CHECK(strcmp(resp(), "ERR STATE\r\n") == 0 && Pwm_GetFreq() == 1000U);
        cmd("HELP");        CHECK(strstr(resp(), "STREAM ON | OFF") != NULL);                      /* HELP, STATUS y STREAM siguen */
        cmd("STATUS");      CHECK(strstr(resp(), "OK mode=PAUSA freq=1000 duty=0 manual=25 ") != NULL && strstr(resp(), " resume=AUTO ") != NULL);
        cmd("STREAM ON");   CHECK(strcmp(resp(), "OK STREAM ON\r\n") == 0);
        cmd("STREAM OFF");  CHECK(strcmp(resp(), "OK STREAM OFF\r\n") == 0);
        cmd("DUTY 20abc");  CHECK(strcmp(resp(), "ERR ARG\r\n") == 0);                              /* y los errores de sintaxis siguen siendo los suyos */
        cmd("FOO");         CHECK(strcmp(resp(), "ERR COMMAND\r\n") == 0);
        cmd("RESUME");      CHECK(strncmp(resp(), "OK RESUME mode=AUTO duty=", 25) == 0 && info().mode == MODE_AUTO);
        CHECK(Pwm_GetDuty() == d + 0U || Pwm_GetDuty() == info().rampDuty);
        cmd("RESUME");      CHECK(strcmp(resp(), "ERR STATE\r\n") == 0);
    }
    cmd("MODE MANUAL");
    cmd("DUTY 60"); cmd("PAUSE"); CHECK(info().resumeTo == MODE_MANUAL);
    cmd("RESUME"); CHECK(strcmp(resp(), "OK RESUME mode=MANUAL duty=60\r\n") == 0 && Pwm_GetDuty() == 60U);
    cmd("DUTY 25");

    /* --- CR, LF y CRLF: una ejecucion por orden --- */
    clear(); feed("STATUS\r"); ms(10); CHECK(count("OK mode=") == 1 && count("ERR") == 0);
    clear(); feed("STATUS\n"); ms(10); CHECK(count("OK mode=") == 1 && count("ERR") == 0);
    clear(); feed("STATUS\r\n"); ms(10); CHECK(count("OK mode=") == 1 && count("ERR") == 0);
    clear(); feed("STATUS\r\nSTATUS\r\nSTATUS\r\n"); ms(30); CHECK(count("OK mode=") == 3 && count("ERR") == 0);
    clear(); feed("\r\n\r\n\n\r"); ms(10); CHECK(count("OK") == 0 && count("ERR") == 0);   /* lineas vacias: ni respuesta ni error */
    clear(); feed("DUTY 30\r\nDUTY 40\nDUTY 50\r"); ms(30); CHECK(count("OK DUTY") == 3 && Pwm_GetDuty() == 50U);
    cmd("DUTY 25");

    /* --- limites de la linea: 63 caracteres si, 64 no --- */
    {
        char l[130];
        memset(l, ' ', 63); memcpy(l, "STATUS", 6); l[63] = 0;
        cmd(l); CHECK(strstr(resp(), "OK mode=") != NULL);
        memset(l, ' ', 64); memcpy(l, "STATUS", 6); l[64] = 0;
        cmd(l); CHECK(strstr(resp(), "ERR OVERFLOW\r\n") != NULL && strstr(resp(), "OK mode=") == NULL);
        /* linea de 100 caracteres y luego STATUS */
        memset(l, 'A', 100); l[100] = 0;
        clear(); feed(l); feed("\r"); feed("STATUS\r"); ms(30);
        CHECK(count("ERR OVERFLOW\r\n") == 1 && count("OK mode=") == 1);
        CHECK(strstr(out(), "ERR OVERFLOW") < strstr(out(), "OK mode="));
        /* una orden valida seguida de relleno: no se ejecuta ningun fragmento */
        clear(); feed("DUTY 90"); memset(l, ' ', 80); l[80] = 0; feed(l); feed("\r"); ms(10);
        CHECK(count("ERR OVERFLOW\r\n") == 1 && Pwm_GetDuty() == 25U);
        clear(); feed("DUTY 91 "); memset(l, 'A', 70); l[70] = 0; feed(l); feed("\r\nDUTY 92\r\n"); ms(20);
        CHECK(count("ERR OVERFLOW\r\n") == 1 && count("OK DUTY 92") == 1 && Pwm_GetDuty() == 92U);
        cmd("DUTY 25");
        CHECK(sh().errOverflow == 4);
        /* 5000 caracteres seguidos sin terminador (con la consola atendiendo) y luego una orden */
        clear();
        for (int i = 0; i < 5000; i++) { sim_rx_byte('Z'); if (i % 20 == 19) ms(2); }
        feed("\r"); ms(10);
        CHECK(count("ERR OVERFLOW\r\n") == 1);
        cmd("STATUS"); CHECK(strstr(resp(), "OK mode=") != NULL);
    }

    /* --- retroceso: DUTY 5X <BS> 0 --- */
    clear(); feed("DUTY 5X\b0\r"); ms(10);
    CHECK(strstr(out(), "\b \b") != NULL && strstr(out(), "OK DUTY 50\r\n") != NULL && Pwm_GetDuty() == 50U);
    cmd("DUTY 25");

    /* --- caracteres invalidos --- */
    clear(); feed("DUTY \x01" "5\r"); ms(10); CHECK(strstr(out(), "ERR COMMAND\r\n") != NULL && Pwm_GetDuty() == 25U);
    clear(); sim_rx_byte('S'); sim_rx_byte(0xC3); sim_rx_byte('\r'); ms(10); CHECK(strstr(out(), "ERR COMMAND\r\n") != NULL);
    cmd("STATUS"); CHECK(strstr(resp(), "OK mode=") != NULL);

    /* --- latencia: el primer byte de la respuesta sale dentro de 100 ms del terminador (aqui, 1 ms) --- */
    {
        clear(); feed("STATUS");
        sim_rx_byte('\r');
        uint32_t t = 0;
        while (strstr(out(), "OK mode=") == NULL && t < 100U) { ms(1); t++; }
        CHECK(t <= 2U);
    }

    /* --- STREAM: una linea TEL cada 1000 ms, sin deriva; sigue en PAUSA --- */
    cmd("STREAM ON"); clear();
    {
        uint32_t t0 = Tick_Ms();
        ms(10500);
        CHECK(count("TEL up=") == 10);
        unsigned long prev = 0; int n = 0; const char *p = out();
        while ((p = strstr(p, "TEL up=")) != NULL) {
            unsigned long u = strtoul(p + 7, NULL, 10);
            if (n > 0) CHECK(u - prev == 1000UL);
            prev = u; n++; p += 7;
        }
        CHECK(strstr(out(), "TEL up=") != NULL && strstr(out(), " mode=MANUAL freq=1000 duty=25 manual=25 short=0 long=0\r\n") != NULL);
        (void)t0;
    }
    cmd("PAUSE"); clear(); ms(3000); CHECK(count("TEL up=") == 3 && count(" mode=PAUSA freq=1000 duty=0 manual=25 ") == 3);
    cmd("RESUME");
    cmd("STREAM OFF"); clear(); ms(3000); CHECK(count("TEL") == 0);                   /* STREAM OFF: silencio */
    cmd("STREAM OFF"); CHECK(strcmp(resp(), "OK STREAM OFF\r\n") == 0);
    cmd("STREAM ON"); ms(600); cmd("STREAM ON"); clear(); ms(450);
    CHECK(count("TEL up=") == 1);                                      /* STREAM ON repetido no reinicia la cuenta: la linea sale a los 1000 ms del primero */
    cmd("STREAM OFF");

    /* --- RX: se pierden bytes (buffer lleno) mientras main no lee: se cuenta y se recupera --- */
    {
        ConsoleStats cs0, cs1; Console_GetStats(&cs0);
        cmd("DIAG STALL 500"); CHECK(strcmp(resp(), "OK DIAG STALL 500\r\n") == 0 && sh().stalled);
        clear();
        /* 40 ordenes DUTY 80 mientras main esta "ocupado": 320 bytes en un buffer de 255 */
        for (int i = 0; i < 40; i++) feed("DUTY 80\r");
        ms(300);
        CHECK(sim_tx_out_len == 0 && Pwm_GetDuty() == 25U);          /* main no ha leido nada */
        ms(300);                                                       /* termina la espera */
        Console_GetStats(&cs1);
        CHECK(cs1.rxRingOverflow - cs0.rxRingOverflow == 320U - 255U);
        CHECK(Pwm_GetDuty() == 25U && info().dutyManual == 25U);      /* NINGUN fragmento se ejecuto */
        CHECK(count("OK DUTY") == 0);
        CHECK(sh().rxFaultDiscards == 1 && !sh().stalled);
        /* el siguiente fin de linea cierra la linea afectada con ERR OVERFLOW; luego todo funciona */
        clear(); feed("STATUS\r"); ms(10);
        CHECK(strstr(out(), "ERR OVERFLOW\r\n") != NULL && count("OK mode=") == 0);
        cmd("STATUS");
        CHECK(strstr(resp(), "OK mode=MANUAL") != NULL && strstr(resp(), " err_ovf=6 rx_lost=65 ") != NULL);
        cmd("DUTY 80"); CHECK(strcmp(resp(), "OK DUTY 80\r\n") == 0);
        cmd("DUTY 25");
    }
    /* error de hardware en medio de una linea (trama, ruido, paridad, overrun) */
    {
        static const uint32_t errs[] = { HAL_UART_ERROR_FE, HAL_UART_ERROR_NE, HAL_UART_ERROR_PE, HAL_UART_ERROR_ORE };
        for (size_t i = 0; i < 4; i++) {
            clear(); feed("DUT"); ms(5); sim_rx_error(errs[i]); ms(2); sim_rx_byte('Y'); feed(" 90\r"); ms(10);
            CHECK(strstr(out(), "ERR OVERFLOW\r\n") != NULL && Pwm_GetDuty() == 25U);
            cmd("STATUS"); CHECK(strstr(resp(), "OK mode=MANUAL") != NULL);                         /* recupera en la linea siguiente */
        }
        cmd("STATUS"); CHECK(strstr(resp(), " uart_err=5 ") != NULL);
    }

    /* --- contrapresion: la cola TX llena no pierde ni corta respuestas --- */
    {
        ConsoleStats c0, c1; Console_GetStats(&c0);
        clear();
        /* 6 HELP seguidos (cada uno > 700 bytes) sin que la ISR de TX avance: no caben todos en la cola */
        for (int i = 0; i < 6; i++) feed("HELP\r");
        for (int k = 0; k < 300; k++) { sim_tick(1); sim_tim2_enter(); App_Loop(); sim_tim2_leave(); }   /* sin drenar */
        int sent_before = (int)sim_tx_out_len;
        CHECK(sim_tx_calls > 0);
        for (int k = 0; k < 400; k++) { ms(1); }                                                          /* ahora la ISR de TX avanza */
        CHECK((int)sim_tx_out_len > sent_before);
        CHECK(count("HELP\r\n") == 6);                                                                    /* ecos */
        CHECK(count("STREAM ON | OFF") == 6);                                                              /* seis HELP completos */
        { const char *p = out(); int okc = 0; while ((p = strstr(p, "\r\nOK\r\n")) != NULL) { okc++; p += 6; } CHECK(okc == 6); }
        Console_GetStats(&c1);
        CHECK(c1.txDropped == c0.txDropped);                                                               /* nada se perdio */
        CHECK(c1.rxRingOverflow == c0.rxRingOverflow || c1.rxRingOverflow - c0.rxRingOverflow == 0);
    }

    /* --- diagnostico: DIAG MEASURE y DIAG PWMALL --- */
    cmd("DIAG MEASURE");
    CHECK(strstr(out(), "OK DIAG MEASURE (TIM3: reloj 84000000 Hz, PSC=83, ARR=999, CCR=250)\r\n") != NULL);
    clear(); ms(300);
    CHECK(strstr(out(), "[medida] 1000 Hz duty  25 % -> f=1000.0 Hz (+0.00 %) duty=25.00 % (+0.00 pp)") != NULL && strstr(out(), "FALLA") == NULL);
    cmd("MODE AUTO"); cmd("DIAG MEASURE"); CHECK(strcmp(resp(), "ERR STATE\r\n") == 0);              /* en AUTO el duty cambia: no se mide */
    cmd("PAUSE"); cmd("DIAG PWMALL"); CHECK(strcmp(resp(), "ERR STATE\r\n") == 0);
    cmd("RESUME"); cmd("MODE MANUAL");
    cmd("DIAG PWMALL"); CHECK(strcmp(resp(), "OK DIAG PWMALL\r\n") == 0 || strstr(out(), "OK DIAG PWMALL\r\n") != NULL);
    /* mientras corre la verificacion, las ordenes que cambian el estado dan ERR STATE */
    cmd("MODE AUTO"); CHECK(strstr(out(), "ERR STATE\r\n") != NULL);
    cmd("DUTY 70");   CHECK(strstr(out(), "ERR STATE\r\n") != NULL);
    cmd("FREQ 500");  CHECK(strstr(out(), "ERR STATE\r\n") != NULL);
    cmd("PAUSE");     CHECK(strstr(out(), "ERR STATE\r\n") != NULL);
    cmd("DIAG WRAP"); CHECK(strstr(out(), "ERR STATE\r\n") != NULL);
    cmd("STATUS");    CHECK(strstr(out(), "OK mode=MANUAL freq=") != NULL);                           /* STATUS si */
    clear(); ms(4000);
    CHECK(strstr(out(), "Resultado: 15/15 OK") != NULL && strstr(out(), "FALLA") == NULL);
    CHECK(Pwm_GetFreq() == 1000U && Pwm_GetDuty() == 25U && info().mode == MODE_MANUAL);
    cmd("DUTY 30"); CHECK(strcmp(resp(), "OK DUTY 30\r\n") == 0);                                    /* y ya acepta ordenes */
    cmd("DUTY 25");

    /* --- DIAG WRAP: el tiempo salta a 2048 ms de UINT32_MAX; todo sigue funcionando al cruzarlo --- */
    cmd("MODE AUTO"); cmd("STREAM ON"); ms(1500);
    cmd("DIAG WRAP");
    CHECK(strstr(out(), "OK DIAG WRAP tick=4294965248 (UINT32_MAX en 2048 ms)\r\n") != NULL);
    CHECK(count("TEL up=") == 0);                                      /* el salto de tiempo no dispara telemetria de mas */
    CHECK(Tick_Ms() >= 0xFFFFF800U);
    {
        uint32_t prevDuty = Pwm_GetDuty(); int jumps = 0;
        clear();
        for (int k = 0; k < 5000; k++) {
            ms(1);
            uint32_t d = Pwm_GetDuty();
            if ((d > prevDuty ? d - prevDuty : prevDuty - d) > 1U) jumps++;
            prevDuty = d;
        }
        CHECK(jumps == 0);                                         /* la rampa no se descontrola al cruzar */
        CHECK(Tick_Ms() < 5000U);                                  /* el contador dio la vuelta */
        CHECK(count("TEL up=") >= 4 && count("TEL up=") <= 5);     /* la telemetria sigue cada 1000 ms */
        { unsigned long prev = 0; int n = 0; const char *p = out();
          while ((p = strstr(p, "TEL up=")) != NULL) {
              unsigned long u = strtoul(p + 7, NULL, 10);
              if (n > 0) CHECK((uint32_t)(u - prev) == 1000UL);   /* (wrap-safe) */
              prev = u; n++; p += 7; } }
    }
    cmd("STREAM OFF"); cmd("MODE MANUAL");

    /* --- DIAG STALL a caballo del desbordamiento: la espera termina cuando toca, no antes --- */
    cmd("DIAG WRAP"); ms(500);
    cmd("DIAG STALL 3000"); clear();
    feed("STATUS\r");
    ms(2500);
    CHECK(Tick_Ms() < 1500U);                                      /* el contador ya cruzo UINT32_MAX */
    CHECK(sim_tx_out_len == 0);                                    /* y main sigue sin leer RX */
    ms(1000);
    CHECK(strstr(out(), "OK mode=") != NULL && !sh().stalled);

    /* --- telemetria atrasada porque la cola TX estaba llena: vuelve a la rejilla de 1000 ms --- */
    cmd("STREAM ON"); clear();
    ms(900);
    { static char big[1000]; memset(big, 'x', sizeof big - 1); big[sizeof big - 1] = 0; CHECK(Console_Write(big)); }
    for (int k = 0; k < 300; k++) { sim_tick(1); sim_tim2_enter(); App_Loop(); sim_tim2_leave(); }   /* la ISR de TX no avanza */
    CHECK(count("TEL up=") == 0);                                  /* venció a los 1000 ms pero no cupo: no se pierde, se reintenta */
    ms(900);
    {
        const char *p1 = strstr(out(), "TEL up="); CHECK(p1 != NULL);
        const char *p2 = p1 ? strstr(p1 + 7, "TEL up=") : NULL; CHECK(p2 != NULL);
        if (p1 && p2) {
            unsigned long u1 = strtoul(p1 + 7, NULL, 10), u2 = strtoul(p2 + 7, NULL, 10);
            CHECK(u2 - u1 > 700UL && u2 - u1 < 950UL);             /* la segunda cae en la rejilla (+2000), no 1000 ms despues de la tardia */
        }
        CHECK(count("TEL up=") == 2);
    }
    cmd("STREAM OFF");
    /* mas de 3 s con la cola llena: al liberarse sale UNA linea, no una rafaga para "recuperar" las perdidas */
    cmd("STREAM ON"); clear();
    { static char big[1000]; memset(big, 'x', sizeof big - 1); big[sizeof big - 1] = 0; CHECK(Console_Write(big)); }
    for (int k = 0; k < 3100; k++) { sim_tick(1); sim_tim2_enter(); App_Loop(); sim_tim2_leave(); }
    CHECK(count("TEL up=") == 0);
    ms(50);
    CHECK(count("TEL up=") == 1);
    ms(1000); CHECK(count("TEL up=") == 2);
    cmd("STREAM OFF");

    /* --- limite de trabajo por vuelta: 100 bytes pendientes no se atienden en una sola vuelta --- */
    clear();
    for (int i = 0; i < 100; i++) sim_rx_byte('x');
    sim_tim2_enter(); App_Loop(); sim_tim2_leave(); sim_tx_drain();
    CHECK(sim_tx_out_len == APP_BYTES_PER_LOOP);                   /* solo se leyeron (y repitieron) 32 */
    ms(5); CHECK(sim_tx_out_len == 100U);                          /* el resto, en las vueltas siguientes */
    feed("\r"); ms(10); CHECK(count("ERR OVERFLOW\r\n") == 1);
    cmd("STATUS"); CHECK(strstr(resp(), "OK mode=") != NULL);

    /* --- ninguna escritura insegura de ARR/CCR en toda la sesion --- */
    CHECK(sim_unsafe_writes == 0);
    printf(fails ? "PROTOCOLO: %d FALLOS\n" : "PROTOCOLO: todas las pruebas OK\n", fails);
    return fails != 0;
}
