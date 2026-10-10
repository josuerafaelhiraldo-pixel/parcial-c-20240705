/* cmd.h - Analisis de una linea de orden (sin HAL, sin esperas, sin atoi).
 *
 * Reglas:
 *  - Las ordenes y sus palabras son MAYUSCULAS (MODE AUTO, STREAM ON...). "status" -> ERR COMMAND.
 *  - Se permiten espacios y tabuladores al inicio, al final y entre palabras.
 *  - No se admiten argumentos adicionales ni faltantes -> ERR ARG.
 *  - Los numeros se validan completos: solo digitos. "20abc", "5.5", "+5", "0x10", "1e2" -> ERR ARG.
 *    Un signo menos seguido de digitos es un entero negativo -> ERR RANGE. Una cadena de digitos
 *    enorme (mas de lo que cabe en 32 bits) -> ERR RANGE. Los ceros a la izquierda se aceptan.
 *  - DUTY n: 0..100 (si no, ERR RANGE). FREQ n: 500, 1000 o 2000 (si no, ERR RANGE).
 *  - El orden de las comprobaciones es: orden conocida (ERR COMMAND), argumentos bien formados
 *    (ERR ARG), valor dentro de rango (ERR RANGE). Despues, el interprete comprueba si el estado
 *    permite la orden (ERR STATE).
 *
 * Cmd_Parse no modifica la linea ni guarda punteros a ella. */
#ifndef CMD_H
#define CMD_H

#include <stdint.h>

typedef enum {
    CMD_HELP = 0,
    CMD_STATUS,
    CMD_MODE_MANUAL,
    CMD_MODE_AUTO,
    CMD_DUTY,
    CMD_FREQ,
    CMD_PAUSE,
    CMD_RESUME,
    CMD_STREAM_ON,
    CMD_STREAM_OFF,
    CMD_DIAG_MEASURE,       /* diagnostico (APP_DIAG_ENABLED): no forma parte del protocolo */
    CMD_DIAG_PWMALL,
    CMD_DIAG_STALL,
    CMD_DIAG_WRAP
} CmdId;

typedef enum {
    CMD_PARSE_OK = 0,
    CMD_PARSE_COMMAND,      /* ERR COMMAND */
    CMD_PARSE_ARG,          /* ERR ARG */
    CMD_PARSE_RANGE         /* ERR RANGE */
} CmdParse;

typedef struct {
    CmdId    id;
    uint32_t value;         /* DUTY, FREQ y DIAG STALL */
} Cmd;

CmdParse Cmd_Parse(const char *line, Cmd *out);

/* Numero sin signo estricto (se expone para probarlo por separado). */
typedef enum {
    NUM_OK = 0,
    NUM_BAD,                /* no es un numero entero valido */
    NUM_NEG,                /* entero negativo */
    NUM_BIG                 /* no cabe en 32 bits */
} NumStatus;

NumStatus Cmd_ParseNumber(const char *s, uint32_t len, uint32_t *value);

#endif /* CMD_H */
