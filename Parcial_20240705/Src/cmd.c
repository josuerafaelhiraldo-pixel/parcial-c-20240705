/* cmd.c - Analisis de ordenes. Ver cmd.h. */
#include "cmd.h"
#include "app_config.h"
#include "pwm.h"
#include <string.h>

#define MAX_TOKENS  4U

typedef struct {
    const char *p;
    uint32_t    n;
} Tok;

static bool is_blank(char c) { return (c == ' ') || (c == '\t'); }

/* Parte la linea en palabras separadas por espacios o tabuladores. Devuelve cuantas hay
 * (si hay mas de MAX_TOKENS devuelve MAX_TOKENS + 1). */
static uint32_t tokenize(const char *s, Tok *t)
{
    uint32_t n = 0U;

    for (;;) {
        while (is_blank(*s)) {
            s++;
        }
        if (*s == '\0') {
            break;
        }
        if (n == MAX_TOKENS) {
            return MAX_TOKENS + 1U;
        }
        t[n].p = s;
        while ((*s != '\0') && !is_blank(*s)) {
            s++;
        }
        t[n].n = (uint32_t)(s - t[n].p);
        n++;
    }
    return n;
}

static bool tok_is(const Tok *t, const char *word)
{
    return (t->n == (uint32_t)strlen(word)) && (memcmp(t->p, word, t->n) == 0);
}

NumStatus Cmd_ParseNumber(const char *s, uint32_t len, uint32_t *value)
{
    uint32_t i = 0U;
    uint32_t v = 0U;
    bool     neg = false;
    bool     big = false;
    uint32_t first;

    *value = 0U;
    if (len == 0U) {
        return NUM_BAD;
    }
    if (s[0] == '-') {
        neg = true;
        i = 1U;
    }
    if (i >= len) {
        return NUM_BAD;                 /* solo "-" */
    }
    first = i;
    for (; i < len; i++) {              /* 1) toda la cadena debe ser digitos */
        if ((s[i] < '0') || (s[i] > '9')) {
            return NUM_BAD;
        }
    }
    for (i = first; i < len; i++) {     /* 2) valor, con deteccion de desbordamiento de 32 bits */
        uint32_t d = (uint32_t)(s[i] - '0');

        if (v > ((0xFFFFFFFFU - d) / 10U)) {
            big = true;
            break;
        }
        v = (v * 10U) + d;
    }
    if (neg) {
        return NUM_NEG;
    }
    if (big) {
        return NUM_BIG;
    }
    *value = v;
    return NUM_OK;
}

/* Lee el argumento numerico de la palabra t. Devuelve el estado del analisis. */
static CmdParse number_arg(const Tok *t, uint32_t *v)
{
    switch (Cmd_ParseNumber(t->p, t->n, v)) {
    case NUM_OK:  return CMD_PARSE_OK;
    case NUM_BAD: return CMD_PARSE_ARG;
    default:      return CMD_PARSE_RANGE;   /* negativo o enorme */
    }
}

static CmdParse parse_diag(const Tok *t, uint32_t n, Cmd *out)
{
#if APP_DIAG_ENABLED
    CmdParse r;

    if (n < 2U) {
        return CMD_PARSE_ARG;
    }
    if (tok_is(&t[1], "STALL")) {
        if (n != 3U) {
            return CMD_PARSE_ARG;
        }
        r = number_arg(&t[2], &out->value);
        if (r != CMD_PARSE_OK) {
            return r;
        }
        if ((out->value < APP_DIAG_STALL_MIN_MS) || (out->value > APP_DIAG_STALL_MAX_MS)) {
            return CMD_PARSE_RANGE;
        }
        out->id = CMD_DIAG_STALL;
        return CMD_PARSE_OK;
    }
    if (n != 2U) {
        return CMD_PARSE_ARG;
    }
    if (tok_is(&t[1], "MEASURE")) {
        out->id = CMD_DIAG_MEASURE;
    } else if (tok_is(&t[1], "PWMALL")) {
        out->id = CMD_DIAG_PWMALL;
    } else if (tok_is(&t[1], "WRAP")) {
        out->id = CMD_DIAG_WRAP;
    } else {
        return CMD_PARSE_ARG;
    }
    return CMD_PARSE_OK;
#else
    (void)t; (void)n; (void)out;
    return CMD_PARSE_COMMAND;
#endif
}

CmdParse Cmd_Parse(const char *line, Cmd *out)
{
    Tok      t[MAX_TOKENS];
    uint32_t n = tokenize(line, t);
    CmdParse r;

    out->id    = CMD_HELP;
    out->value = 0U;
    if (n == 0U) {
        return CMD_PARSE_COMMAND;       /* no deberia llegar: las lineas vacias no son ordenes */
    }

    if (tok_is(&t[0], "HELP")) {
        out->id = CMD_HELP;
        return (n == 1U) ? CMD_PARSE_OK : CMD_PARSE_ARG;
    }
    if (tok_is(&t[0], "STATUS")) {
        out->id = CMD_STATUS;
        return (n == 1U) ? CMD_PARSE_OK : CMD_PARSE_ARG;
    }
    if (tok_is(&t[0], "PAUSE")) {
        out->id = CMD_PAUSE;
        return (n == 1U) ? CMD_PARSE_OK : CMD_PARSE_ARG;
    }
    if (tok_is(&t[0], "RESUME")) {
        out->id = CMD_RESUME;
        return (n == 1U) ? CMD_PARSE_OK : CMD_PARSE_ARG;
    }
    if (tok_is(&t[0], "MODE")) {
        if (n != 2U) {
            return CMD_PARSE_ARG;
        }
        if (tok_is(&t[1], "MANUAL")) {
            out->id = CMD_MODE_MANUAL;
            return CMD_PARSE_OK;
        }
        if (tok_is(&t[1], "AUTO")) {
            out->id = CMD_MODE_AUTO;
            return CMD_PARSE_OK;
        }
        return CMD_PARSE_ARG;
    }
    if (tok_is(&t[0], "STREAM")) {
        if (n != 2U) {
            return CMD_PARSE_ARG;
        }
        if (tok_is(&t[1], "ON")) {
            out->id = CMD_STREAM_ON;
            return CMD_PARSE_OK;
        }
        if (tok_is(&t[1], "OFF")) {
            out->id = CMD_STREAM_OFF;
            return CMD_PARSE_OK;
        }
        return CMD_PARSE_ARG;
    }
    if (tok_is(&t[0], "DUTY")) {
        out->id = CMD_DUTY;
        if (n != 2U) {
            return CMD_PARSE_ARG;
        }
        r = number_arg(&t[1], &out->value);
        if ((r == CMD_PARSE_OK) && (out->value > 100U)) {
            r = CMD_PARSE_RANGE;
        }
        return r;
    }
    if (tok_is(&t[0], "FREQ")) {
        out->id = CMD_FREQ;
        if (n != 2U) {
            return CMD_PARSE_ARG;
        }
        r = number_arg(&t[1], &out->value);
        if ((r == CMD_PARSE_OK) && !Pwm_FreqIsValid(out->value)) {
            r = CMD_PARSE_RANGE;
        }
        return r;
    }
    if (tok_is(&t[0], "DIAG")) {
        return parse_diag(t, n, out);
    }
    return CMD_PARSE_COMMAND;
}
