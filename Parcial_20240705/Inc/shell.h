/* shell.h - Interprete de ordenes por la consola serie y telemetria.
 *
 * Todo ocurre en main, sin esperas: Shell_Service() se llama en cada vuelta. Atiende como mucho
 * APP_BYTES_PER_LOOP bytes de RX y una orden por vuelta. Las respuestas se encolan en la consola
 * (Console_WriteBytes no bloquea); si no caben, la respuesta queda pendiente y no se lee la
 * orden siguiente hasta enviarla (los bytes esperan en el buffer circular de RX).
 *
 * Formato de las respuestas (cada linea termina en CR LF):
 *    OK ...            orden valida (con datos cuando corresponde)
 *    ERR COMMAND | ERR ARG | ERR RANGE | ERR STATE | ERR OVERFLOW
 *    TEL ...           telemetria (STREAM ON), una por cada 1000 ms
 *    EVT ...           eventos de B1 (solo con STREAM ON)
 * Un rechazo nunca cambia parametros. */
#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>
#include <stdbool.h>
#include "button.h"

typedef struct {
    uint32_t cmdOk;             /* ordenes validas ejecutadas */
    uint32_t errCommand;
    uint32_t errArg;
    uint32_t errRange;
    uint32_t errState;
    uint32_t errOverflow;       /* lineas descartadas (demasiado largas o con RX danada) */
    uint32_t rxFaultDiscards;   /* cuantas veces un fallo de RX hizo descartar la linea en curso */
    bool     stream;
    bool     stalled;
} ShellStats;

void Shell_Init(uint32_t now);
void Shell_Service(uint32_t now);
void Shell_ButtonEvent(ButtonEvent ev, bool changed);   /* aviso EVT (solo con STREAM ON) */
void Shell_GetStats(ShellStats *out);

#endif /* SHELL_H */
