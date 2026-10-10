/* line.h - Armado de lineas de orden a partir de los bytes recibidos (sin HAL, sin esperas).
 *
 *  - Terminadores: CR, LF y CRLF. CRLF produce UNA sola orden (el LF que sigue a un CR se ignora).
 *  - Maximo APP_LINE_MAX (63) caracteres por linea, mas el NUL. Si llega un caracter 64, la linea
 *    se marca "demasiado larga": se descarta TODO hasta el siguiente terminador, sin ejecutar
 *    fragmentos, y ese terminador produce LINE_OVERFLOW (el interprete responde ERR OVERFLOW).
 *    La linea siguiente empieza limpia: asi se resincroniza.
 *  - Line_Poison(): lo usa el interprete cuando RX perdio o danio bytes (buffer lleno, ORE, FE, NE,
 *    PE). Abandona la linea en curso y descarta hasta el siguiente terminador.
 *  - Retroceso (BS 0x08 o DEL 0x7F) borra el ultimo caracter. Los demas caracteres de control y los
 *    bytes >= 0x80 invalidan la linea (LINE_BAD). El tabulador cuenta como espacio.
 *  - Los espacios al inicio y al final se permiten; una linea vacia o solo con espacios no es orden. */
#ifndef LINE_H
#define LINE_H

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

typedef enum {
    LINE_IGNORED = 0,   /* byte consumido; no hay nada que repetir (eco) ni que hacer */
    LINE_CHAR,          /* caracter imprimible consumido (guardado o descartado): se puede repetir */
    LINE_ERASE,         /* retroceso: se borro el ultimo caracter */
    LINE_READY,         /* terminador: Line_Text() tiene una orden no vacia */
    LINE_EMPTY,         /* terminador: linea vacia (o solo espacios): no es una orden */
    LINE_OVERFLOW,      /* terminador de una linea demasiado larga o danada: responder ERR OVERFLOW */
    LINE_BAD            /* terminador de una linea con caracteres invalidos: responder ERR COMMAND */
} LineEvent;

typedef struct {
    char    buf[APP_LINE_MAX + 1U];
    uint8_t len;
    bool    ready;      /* buf tiene una orden entregada; se limpia con el siguiente byte */
    bool    prevCR;     /* el byte anterior fue CR */
    bool    tooLong;
    bool    damaged;
    bool    bad;
} LineReader;

void        Line_Init(LineReader *lr);
LineEvent   Line_Feed(LineReader *lr, uint8_t c);
const char *Line_Text(const LineReader *lr);      /* valido tras LINE_READY, hasta el siguiente Line_Feed */
void        Line_Poison(LineReader *lr);

#endif /* LINE_H */
