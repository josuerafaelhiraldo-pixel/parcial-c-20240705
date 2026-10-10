/* Pruebas en PC de cmd.c: tabla de ordenes validas e invalidas, validacion estricta de numeros
 * y una prueba con lineas al azar. */
#include "cmd.h"
#include "hal_sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALLO linea %d: %s\n", __LINE__, #c); fails++; } } while (0)

/* line, resultado esperado, id esperado, valor esperado (solo si resultado = OK) */
typedef struct { const char *line; CmdParse r; CmdId id; uint32_t v; } Case;
#define OK_(l, id, v) { l, CMD_PARSE_OK, id, v }
#define ER_(l, r)     { l, r, CMD_HELP, 0 }

static const Case cases[] = {
    /* ordenes validas */
    OK_("HELP", CMD_HELP, 0), OK_("STATUS", CMD_STATUS, 0), OK_("PAUSE", CMD_PAUSE, 0), OK_("RESUME", CMD_RESUME, 0),
    OK_("MODE MANUAL", CMD_MODE_MANUAL, 0), OK_("MODE AUTO", CMD_MODE_AUTO, 0),
    OK_("STREAM ON", CMD_STREAM_ON, 0), OK_("STREAM OFF", CMD_STREAM_OFF, 0),
    OK_("DUTY 0", CMD_DUTY, 0), OK_("DUTY 50", CMD_DUTY, 50), OK_("DUTY 100", CMD_DUTY, 100),
    OK_("FREQ 500", CMD_FREQ, 500), OK_("FREQ 1000", CMD_FREQ, 1000), OK_("FREQ 2000", CMD_FREQ, 2000),
    /* espacios al inicio y al final, y varios entre palabras */
    OK_("  STATUS", CMD_STATUS, 0), OK_("STATUS   ", CMD_STATUS, 0), OK_("   STATUS \t ", CMD_STATUS, 0),
    OK_("DUTY    75", CMD_DUTY, 75), OK_("\tDUTY\t75\t", CMD_DUTY, 75), OK_("  MODE   AUTO  ", CMD_MODE_AUTO, 0),
    /* ceros a la izquierda: son digitos validos */
    OK_("DUTY 007", CMD_DUTY, 7), OK_("DUTY 000", CMD_DUTY, 0), OK_("DUTY 0100", CMD_DUTY, 100), OK_("FREQ 01000", CMD_FREQ, 1000),

    /* ERR COMMAND: orden desconocida (todo en mayusculas) */
    ER_("FOO", CMD_PARSE_COMMAND), ER_("status", CMD_PARSE_COMMAND), ER_("Status", CMD_PARSE_COMMAND),
    ER_("STATUSX", CMD_PARSE_COMMAND), ER_("STAT", CMD_PARSE_COMMAND), ER_("duty 50", CMD_PARSE_COMMAND),
    ER_("HELP2", CMD_PARSE_COMMAND), ER_("MODES AUTO", CMD_PARSE_COMMAND), ER_("123", CMD_PARSE_COMMAND),
    ER_("?", CMD_PARSE_COMMAND), ER_("DUTY50", CMD_PARSE_COMMAND), ER_("MODEAUTO", CMD_PARSE_COMMAND),

    /* ERR ARG: argumentos faltantes, sobrantes o mal formados */
    ER_("HELP me", CMD_PARSE_ARG), ER_("STATUS 1", CMD_PARSE_ARG), ER_("PAUSE now", CMD_PARSE_ARG), ER_("RESUME 1", CMD_PARSE_ARG),
    ER_("MODE", CMD_PARSE_ARG), ER_("MODE FOO", CMD_PARSE_ARG), ER_("MODE manual", CMD_PARSE_ARG), ER_("MODE AUTO X", CMD_PARSE_ARG),
    ER_("MODE MANUAL AUTO", CMD_PARSE_ARG), ER_("MODE 1", CMD_PARSE_ARG),
    ER_("STREAM", CMD_PARSE_ARG), ER_("STREAM on", CMD_PARSE_ARG), ER_("STREAM ON OFF", CMD_PARSE_ARG), ER_("STREAM 1", CMD_PARSE_ARG),
    ER_("DUTY", CMD_PARSE_ARG), ER_("DUTY ", CMD_PARSE_ARG), ER_("DUTY 20abc", CMD_PARSE_ARG), ER_("DUTY abc", CMD_PARSE_ARG),
    ER_("DUTY 5.5", CMD_PARSE_ARG), ER_("DUTY +5", CMD_PARSE_ARG), ER_("DUTY 0x10", CMD_PARSE_ARG), ER_("DUTY 1e2", CMD_PARSE_ARG),
    ER_("DUTY -", CMD_PARSE_ARG), ER_("DUTY -abc", CMD_PARSE_ARG), ER_("DUTY --5", CMD_PARSE_ARG), ER_("DUTY 5-", CMD_PARSE_ARG),
    ER_("DUTY 10 20", CMD_PARSE_ARG), ER_("DUTY 10 20 30 40 50", CMD_PARSE_ARG), ER_("DUTY 1,5", CMD_PARSE_ARG),
    ER_("DUTY 99999999999999999999x", CMD_PARSE_ARG), ER_("DUTY 12 abc", CMD_PARSE_ARG),
    ER_("FREQ", CMD_PARSE_ARG), ER_("FREQ 1000Hz", CMD_PARSE_ARG), ER_("FREQ abc", CMD_PARSE_ARG), ER_("FREQ 1000 1", CMD_PARSE_ARG),
    ER_("FREQ 1k", CMD_PARSE_ARG),

    /* ERR RANGE: numero bien formado pero fuera de lo permitido */
    ER_("DUTY 101", CMD_PARSE_RANGE), ER_("DUTY 200", CMD_PARSE_RANGE), ER_("DUTY 1000", CMD_PARSE_RANGE),
    ER_("DUTY -1", CMD_PARSE_RANGE), ER_("DUTY -0", CMD_PARSE_RANGE), ER_("DUTY -100", CMD_PARSE_RANGE),
    ER_("DUTY 4294967295", CMD_PARSE_RANGE), ER_("DUTY 4294967296", CMD_PARSE_RANGE),
    ER_("DUTY 99999999999999999999", CMD_PARSE_RANGE), ER_("DUTY -99999999999999999999", CMD_PARSE_RANGE),
    ER_("DUTY 4294967396", CMD_PARSE_RANGE),                     /* 2^32 + 100: no debe dar 100 por desbordamiento */
    ER_("DUTY 18446744073709551716", CMD_PARSE_RANGE),           /* 2^64 + 100 */
    ER_("FREQ 0", CMD_PARSE_RANGE), ER_("FREQ 1", CMD_PARSE_RANGE), ER_("FREQ 750", CMD_PARSE_RANGE),
    ER_("FREQ 499", CMD_PARSE_RANGE), ER_("FREQ 501", CMD_PARSE_RANGE), ER_("FREQ 1500", CMD_PARSE_RANGE),
    ER_("FREQ 2001", CMD_PARSE_RANGE), ER_("FREQ 4000", CMD_PARSE_RANGE), ER_("FREQ -1000", CMD_PARSE_RANGE),
    ER_("FREQ 4294967296", CMD_PARSE_RANGE), ER_("FREQ 4294968296", CMD_PARSE_RANGE),   /* 2^32 + 1000 */
    ER_("FREQ 99999999999999999999", CMD_PARSE_RANGE),
};

int main(void)
{
    Cmd c;

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        const Case *k = &cases[i];
        CmdParse r = Cmd_Parse(k->line, &c);
        if (r != k->r) { printf("  caso '%s': esperado %d, obtenido %d\n", k->line, (int)k->r, (int)r); }
        CHECK(r == k->r);
        if ((r == CMD_PARSE_OK) && (k->r == CMD_PARSE_OK)) {
            if (c.id != k->id || c.value != k->v) { printf("  caso '%s': id %d valor %lu\n", k->line, (int)c.id, (unsigned long)c.value); }
            CHECK(c.id == k->id);
            CHECK(c.value == k->v);
        }
    }

    /* la linea no se modifica */
    { char l[] = "  DUTY   42  "; char copy[sizeof l]; memcpy(copy, l, sizeof l);
      CHECK(Cmd_Parse(l, &c) == CMD_PARSE_OK && c.value == 42U); CHECK(memcmp(l, copy, sizeof l) == 0); }

    /* --- DIAG (APP_DIAG_ENABLED = 1) --- */
    CHECK(Cmd_Parse("DIAG MEASURE", &c) == CMD_PARSE_OK && c.id == CMD_DIAG_MEASURE);
    CHECK(Cmd_Parse("DIAG PWMALL", &c) == CMD_PARSE_OK && c.id == CMD_DIAG_PWMALL);
    CHECK(Cmd_Parse("DIAG WRAP", &c) == CMD_PARSE_OK && c.id == CMD_DIAG_WRAP);
    CHECK(Cmd_Parse("DIAG STALL 1000", &c) == CMD_PARSE_OK && c.id == CMD_DIAG_STALL && c.value == 1000U);
    CHECK(Cmd_Parse("DIAG STALL 100", &c) == CMD_PARSE_OK && c.value == 100U);
    CHECK(Cmd_Parse("DIAG STALL 5000", &c) == CMD_PARSE_OK && c.value == 5000U);
    CHECK(Cmd_Parse("DIAG STALL 99", &c) == CMD_PARSE_RANGE);
    CHECK(Cmd_Parse("DIAG STALL 5001", &c) == CMD_PARSE_RANGE);
    CHECK(Cmd_Parse("DIAG STALL -5", &c) == CMD_PARSE_RANGE);
    CHECK(Cmd_Parse("DIAG STALL", &c) == CMD_PARSE_ARG);
    CHECK(Cmd_Parse("DIAG STALL abc", &c) == CMD_PARSE_ARG);
    CHECK(Cmd_Parse("DIAG STALL 1000 1", &c) == CMD_PARSE_ARG);
    CHECK(Cmd_Parse("DIAG", &c) == CMD_PARSE_ARG);
    CHECK(Cmd_Parse("DIAG FOO", &c) == CMD_PARSE_ARG);
    CHECK(Cmd_Parse("DIAG MEASURE 1", &c) == CMD_PARSE_ARG);
    CHECK(Cmd_Parse("DIAG WRAP now", &c) == CMD_PARSE_ARG);
    CHECK(Cmd_Parse("diag wrap", &c) == CMD_PARSE_COMMAND);

    /* --- Cmd_ParseNumber directo --- */
    {
        uint32_t v;
        CHECK(Cmd_ParseNumber("0", 1, &v) == NUM_OK && v == 0U);
        CHECK(Cmd_ParseNumber("4294967295", 10, &v) == NUM_OK && v == 4294967295U);
        CHECK(Cmd_ParseNumber("4294967296", 10, &v) == NUM_BIG);
        CHECK(Cmd_ParseNumber("42949672950", 11, &v) == NUM_BIG);
        CHECK(Cmd_ParseNumber("", 0, &v) == NUM_BAD);
        CHECK(Cmd_ParseNumber("-", 1, &v) == NUM_BAD);
        CHECK(Cmd_ParseNumber("-5", 2, &v) == NUM_NEG);
        CHECK(Cmd_ParseNumber("-99999999999", 12, &v) == NUM_NEG);
        CHECK(Cmd_ParseNumber("12a", 3, &v) == NUM_BAD);
        CHECK(Cmd_ParseNumber("12 ", 3, &v) == NUM_BAD);            /* la longitud manda: el espacio no es digito */
        CHECK(Cmd_ParseNumber("123", 2, &v) == NUM_OK && v == 12U); /* solo se leen len caracteres */
        CHECK(Cmd_ParseNumber("1a", 1, &v) == NUM_OK && v == 1U);
        CHECK(Cmd_ParseNumber("99999999999999999999999999999a", 30, &v) == NUM_BAD);   /* basura despues de un numero enorme */
        CHECK(Cmd_ParseNumber("/", 1, &v) == NUM_BAD && Cmd_ParseNumber(":", 1, &v) == NUM_BAD);   /* vecinos de '0' y '9' */
    }

    /* --- lineas al azar: nunca se sale de la linea ni cambia el resultado entre llamadas --- */
    {
        const char alpha[] = "DUTYFREQMODEAUTOMANUALPAUSERESUMESTREAMONOFFHELPSTATUSDIAG 0123456789-+.x\t";
        srand(99);
        for (int i = 0; i < 300000; i++) {
            char l[64]; int n = rand() % 63;
            for (int k = 0; k < n; k++) l[k] = alpha[rand() % (int)(sizeof alpha - 1)];
            l[n] = 0;
            CmdParse r1 = Cmd_Parse(l, &c); Cmd c2; CmdParse r2 = Cmd_Parse(l, &c2);
            CHECK(r1 == r2);
            if (r1 == CMD_PARSE_OK && c.id == CMD_DUTY) CHECK(c.value <= 100U);
            if (r1 == CMD_PARSE_OK && c.id == CMD_FREQ) CHECK(c.value == 500U || c.value == 1000U || c.value == 2000U);
        }
    }

    printf(fails ? "CMD: %d FALLOS\n" : "CMD: todas las pruebas OK\n", fails);
    return fails != 0;
}
