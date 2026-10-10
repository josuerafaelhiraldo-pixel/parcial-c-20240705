/* Pruebas en PC de line.c: terminadores CR/LF/CRLF, 63 caracteres, desbordamiento de linea,
 * resincronizacion, retroceso, caracteres invalidos, Line_Poison y una prueba con bytes al azar. */
#include "line.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALLO linea %d: %s\n", __LINE__, #c); fails++; } } while (0)

static LineReader lr;

/* Alimenta s y devuelve los eventos de "terminador" en orden (R=READY, E=EMPTY, O=OVERFLOW, B=BAD),
 * en texts[] el texto de cada READY. */
static char evs[256];
static char texts[16][80];
static int  ntexts;
static void feed(const char *s, size_t n)
{
    size_t k = 0;
    evs[0] = 0; ntexts = 0;
    for (size_t i = 0; i < n; i++) {
        LineEvent e = Line_Feed(&lr, (uint8_t)s[i]);
        switch (e) {
        case LINE_READY: evs[k++] = 'R'; if (ntexts < 16) { strncpy(texts[ntexts], Line_Text(&lr), 79); texts[ntexts][79] = 0; ntexts++; } break;
        case LINE_EMPTY: evs[k++] = 'E'; break;
        case LINE_OVERFLOW: evs[k++] = 'O'; break;
        case LINE_BAD: evs[k++] = 'B'; break;
        default: break;
        }
        evs[k] = 0;
    }
}
static void feeds(const char *s) { feed(s, strlen(s)); }

int main(void)
{
    Line_Init(&lr);

    /* --- terminadores --- */
    feeds("STATUS\r");     CHECK(strcmp(evs, "R") == 0 && strcmp(texts[0], "STATUS") == 0);
    feeds("STATUS\n");     CHECK(strcmp(evs, "R") == 0 && strcmp(texts[0], "STATUS") == 0);
    feeds("STATUS\r\n");   CHECK(strcmp(evs, "R") == 0);                   /* CRLF = una sola orden */
    feeds("STATUS\n\r");   CHECK(strcmp(evs, "RE") == 0);                  /* LF y luego CR: la segunda es una linea vacia */
    feeds("A\rB\nC\r\nD\n"); CHECK(strcmp(evs, "RRRR") == 0 && strcmp(texts[2], "C") == 0);
    feeds("\r");           CHECK(strcmp(evs, "E") == 0);
    feeds("\r\n");         CHECK(strcmp(evs, "E") == 0);                   /* CRLF vacio: una linea vacia, no dos */
    feeds("   \r");        CHECK(strcmp(evs, "E") == 0);
    feeds(" \t \r\n");     CHECK(strcmp(evs, "E") == 0);
    feeds("  STATUS  \r"); CHECK(strcmp(evs, "R") == 0 && strcmp(texts[0], "  STATUS  ") == 0);
    /* CR y LF separados en el tiempo siguen siendo una sola orden */
    feeds("STATUS\r"); CHECK(strcmp(evs, "R") == 0);
    feeds("\n");       CHECK(strcmp(evs, "") == 0);
    feeds("STATUS\r"); CHECK(strcmp(evs, "R") == 0);

    /* --- longitud: 63 caracteres caben, 64 no --- */
    {
        char l[200];
        memset(l, 'A', 63); l[63] = '\r'; feed(l, 64);
        CHECK(strcmp(evs, "R") == 0 && strlen(texts[0]) == 63);
        memset(l, 'A', 64); l[64] = '\r'; feed(l, 65);
        CHECK(strcmp(evs, "O") == 0);                                      /* 64: se descarta toda la linea */
        memset(l, 'A', 100); l[100] = '\r'; feed(l, 101);
        CHECK(strcmp(evs, "O") == 0);
        /* tras el desbordamiento, la linea siguiente empieza limpia: resincroniza */
        feeds("STATUS\r"); CHECK(strcmp(evs, "R") == 0 && strcmp(texts[0], "STATUS") == 0);
        /* la linea larga y la siguiente en el mismo bloque */
        memset(l, 'A', 100); l[100] = '\r'; memcpy(l + 101, "STATUS\r", 7); feed(l, 108);
        CHECK(strcmp(evs, "OR") == 0 && strcmp(texts[0], "STATUS") == 0);
        /* CRLF despues de una linea larga: un solo OVERFLOW */
        memset(l, 'A', 100); l[100] = '\r'; l[101] = '\n'; feed(l, 102);
        CHECK(strcmp(evs, "O") == 0);
        /* una orden valida con relleno: no se ejecuta ningun fragmento */
        feeds("DUTY 50"); memset(l, ' ', 70); l[70] = 'x'; l[71] = '\r'; feed(l, 72);
        CHECK(strcmp(evs, "O") == 0 && ntexts == 0);
        /* 5000 caracteres sin terminador: no hay evento ni desbordamiento de memoria */
        for (int i = 0; i < 5000; i++) { LineEvent e = Line_Feed(&lr, 'Z'); CHECK(e == LINE_CHAR); }
        CHECK(lr.len <= APP_LINE_MAX && lr.buf[lr.len] == '\0');
        feeds("\r"); CHECK(strcmp(evs, "O") == 0);
        feeds("HELP\r"); CHECK(strcmp(evs, "R") == 0);
    }

    /* --- retroceso (BS y DEL) --- */
    {
        LineEvent e;
        e = Line_Feed(&lr, 0x08); CHECK(e == LINE_IGNORED);                /* con la linea vacia: nada */
        feeds("DUTY 5X");
        e = Line_Feed(&lr, 0x08); CHECK(e == LINE_ERASE);
        e = Line_Feed(&lr, 0x7F); CHECK(e == LINE_ERASE);
        feeds("0\r"); CHECK(strcmp(evs, "R") == 0 && strcmp(texts[0], "DUTY 0") == 0);
        feeds("AB\b\b\b\bCD\r"); CHECK(strcmp(evs, "R") == 0 && strcmp(texts[0], "CD") == 0);
        { char l[100]; memset(l, 'A', 70); feed(l, 70);
          CHECK(Line_Feed(&lr, 0x08) == LINE_IGNORED);                     /* en una linea larga el retroceso no borra nada */
          feeds("\r"); CHECK(strcmp(evs, "O") == 0); }                     /* ni la "cura" */
    }

    /* --- caracteres invalidos --- */
    feed("ST\x01" "ATUS\r", 8);    CHECK(strcmp(evs, "B") == 0);
    feed("STATUS\0\r", 8);         CHECK(strcmp(evs, "B") == 0);           /* un NUL no puede truncar la orden */
    feed("STA\xC3\xA9US\r", 8);    CHECK(strcmp(evs, "B") == 0);
    feeds("\x1b[A\r");             CHECK(strcmp(evs, "B") == 0);           /* secuencia de escape (flecha) */
    feeds("STATUS\r");             CHECK(strcmp(evs, "R") == 0);           /* la siguiente linea esta limpia */
    CHECK(Line_Feed(&lr, 0x01) == LINE_IGNORED);
    feeds("\r");                   CHECK(strcmp(evs, "B") == 0);

    /* --- eco: que devuelve cada byte --- */
    Line_Init(&lr);
    CHECK(Line_Feed(&lr, 'S') == LINE_CHAR);
    CHECK(Line_Feed(&lr, '\t') == LINE_CHAR);
    CHECK(Line_Feed(&lr, '\r') == LINE_READY);
    CHECK(Line_Feed(&lr, '\n') == LINE_IGNORED);
    CHECK(Line_Feed(&lr, '\r') == LINE_EMPTY);

    /* --- Line_Poison: fallo de RX en medio de una linea --- */
    feeds("DUT"); Line_Poison(&lr); feeds("Y 90\r");
    CHECK(strcmp(evs, "O") == 0 && ntexts == 0);                           /* no se ejecuta el fragmento */
    feeds("STATUS\r"); CHECK(strcmp(evs, "R") == 0);                        /* y la siguiente linea funciona */
    Line_Poison(&lr);                                                      /* fallo entre lineas: se pierde la siguiente */
    feeds("STATUS\r"); CHECK(strcmp(evs, "O") == 0);
    feeds("STATUS\r"); CHECK(strcmp(evs, "R") == 0);
    Line_Poison(&lr); feeds("\r\n"); CHECK(strcmp(evs, "O") == 0);          /* CRLF despues de poison: un solo evento */
    Line_Poison(&lr); Line_Poison(&lr); feeds("x\r"); CHECK(strcmp(evs, "O") == 0);
    feeds("STATUS\r"); CHECK(strcmp(evs, "R") == 0);

    /* --- bytes al azar: nunca se sale de los limites --- */
    {
        unsigned seed = 12345;
        long ready = 0;
        srand(seed);
        for (long i = 0; i < 2000000; i++) {
            uint8_t b = (uint8_t)(rand() % 256);
            if (rand() % 40 == 0) b = (uint8_t)"\r\n"[rand() % 2];
            LineEvent e = Line_Feed(&lr, b);
            if (e == LINE_READY) {
                ready++;
                const char *t = Line_Text(&lr);
                CHECK(strlen(t) <= APP_LINE_MAX);
                for (const char *q = t; *q; q++) CHECK(((unsigned char)*q >= 32 && (unsigned char)*q <= 126) || *q == '\t');
            }
            CHECK(lr.len <= APP_LINE_MAX && lr.buf[lr.len] == '\0');
        }
        CHECK(ready > 1000);
    }

    printf(fails ? "LINE: %d FALLOS\n" : "LINE: todas las pruebas OK\n", fails);
    return fails != 0;
}
