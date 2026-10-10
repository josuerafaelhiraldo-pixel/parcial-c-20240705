/* line.c - Armado de lineas de orden. Ver line.h. */
#include "line.h"

static void clear_line(LineReader *lr)
{
    lr->len     = 0U;
    lr->buf[0]  = '\0';
    lr->ready   = false;
    lr->tooLong = false;
    lr->damaged = false;
    lr->bad     = false;
}

void Line_Init(LineReader *lr)
{
    clear_line(lr);
    lr->prevCR = false;
}

static bool only_blanks(const LineReader *lr)
{
    uint8_t i;

    for (i = 0U; i < lr->len; i++) {
        if ((lr->buf[i] != ' ') && (lr->buf[i] != '\t')) {
            return false;
        }
    }
    return true;
}

/* Llego un terminador: decide que paso con la linea. */
static LineEvent finish(LineReader *lr)
{
    LineEvent ev;

    if (lr->tooLong || lr->damaged) {
        ev = LINE_OVERFLOW;
        clear_line(lr);
    } else if (lr->bad) {
        ev = LINE_BAD;
        clear_line(lr);
    } else if (only_blanks(lr)) {
        ev = LINE_EMPTY;
        clear_line(lr);
    } else {
        ev = LINE_READY;
        lr->ready = true;               /* el texto queda en buf hasta el siguiente byte */
    }
    return ev;
}

LineEvent Line_Feed(LineReader *lr, uint8_t c)
{
    bool wasCR = lr->prevCR;

    if (lr->ready) {
        clear_line(lr);                 /* la orden anterior ya fue entregada */
    }
    lr->prevCR = (c == (uint8_t)'\r');

    if ((c == (uint8_t)'\n') && wasCR) {
        return LINE_IGNORED;            /* segunda mitad de CRLF: no es otra orden */
    }
    if ((c == (uint8_t)'\r') || (c == (uint8_t)'\n')) {
        return finish(lr);
    }

    if ((c == 0x08U) || (c == 0x7FU)) { /* retroceso */
        if ((lr->len > 0U) && !lr->tooLong && !lr->damaged) {
            lr->len--;
            lr->buf[lr->len] = '\0';
            return LINE_ERASE;
        }
        return LINE_IGNORED;
    }

    if (((c >= 32U) && (c <= 126U)) || (c == (uint8_t)'\t')) {
        if (lr->tooLong || lr->damaged) {
            return LINE_CHAR;           /* descartando hasta el terminador */
        }
        if (lr->len >= APP_LINE_MAX) {
            lr->tooLong = true;         /* caracter 64: la linea completa se descarta */
            return LINE_CHAR;
        }
        lr->buf[lr->len] = (char)c;
        lr->len++;
        lr->buf[lr->len] = '\0';
        return LINE_CHAR;
    }

    lr->bad = true;                     /* NUL u otro control, o byte >= 0x80 */
    return LINE_IGNORED;
}

const char *Line_Text(const LineReader *lr)
{
    return lr->buf;
}

void Line_Poison(LineReader *lr)
{
    clear_line(lr);
    lr->damaged = true;
}
