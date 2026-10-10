/* ctrl.h - Maquina de estados MANUAL / AUTO / PAUSA, rampa y patron del LED de estado.
 *
 *   MANUAL (inicial): PWM al ultimo duty manual (25 % al arrancar); LD2 encendido fijo.
 *   AUTO            : rampa triangular 10 % <-> 90 %, 1 punto cada 20 ms (80 pasos = 1,6 s
 *                     por tramo, 3,2 s el ciclo); LD2 alterna cada 500 ms.
 *                     Al entrar empieza en 10 % y asciende.
 *   PAUSA           : PWM al 0 %; LD2 alterna cada 125 ms. Conserva el modo previo, el duty
 *                     manual, el valor y la direccion de la rampa y la frecuencia. Al reanudar
 *                     NO se recuperan los pasos del tiempo pausado.
 *
 * Todo se hace sin esperas: Ctrl_Service(now) se llama en cada vuelta de main y compara
 * tiempos con diferencias sin signo (uint32_t)(ahora - antes), asi que resiste el
 * desbordamiento del contador de milisegundos. Los instantes se avanzan sumando el periodo
 * (no se reinicia el contador de tiempo) para que la rampa y el LED no acumulen retraso.
 * Las ISR no tocan este modulo. */
#ifndef CTRL_H
#define CTRL_H

#include <stdint.h>
#include <stdbool.h>
#include "button.h"

typedef enum { MODE_MANUAL = 0, MODE_AUTO, MODE_PAUSE } CtrlMode;

typedef enum {
    CTRL_OK = 0,
    CTRL_ERR_STATE,     /* el estado actual no permite la operacion */
    CTRL_ERR_RANGE      /* valor fuera de rango (el interprete ya lo valida; defensa) */
} CtrlResult;

typedef struct {
    CtrlMode mode;
    CtrlMode resumeTo;      /* a donde vuelve RESUME (solo importa en PAUSA) */
    uint32_t freqHz;
    uint32_t dutyApplied;   /* lo que sale por el pin (0 en PAUSA) */
    uint32_t dutyManual;    /* ultimo duty manual guardado */
    uint32_t rampDuty;      /* valor actual de la rampa (se conserva en PAUSA) */
    int8_t   rampDir;       /* +1 asciende, -1 desciende */
    bool     ledOn;
    uint32_t shortIgnored;  /* cortas de B1 ignoradas en PAUSA */
} CtrlInfo;

void        Ctrl_Init(uint32_t now);            /* tras Pwm_Init(): MANUAL, 1 kHz, 25 % */
void        Ctrl_Service(uint32_t now);         /* en cada vuelta de main: rampa y LED */

CtrlResult  Ctrl_SetManual(uint32_t now);       /* MODE MANUAL: restaura el duty manual */
CtrlResult  Ctrl_SetAuto(uint32_t now);         /* MODE AUTO: desde 10 %, ascendiendo */
CtrlResult  Ctrl_SetDuty(uint32_t pct);         /* solo en MANUAL */
CtrlResult  Ctrl_SetFreq(uint32_t hz);          /* en MANUAL y AUTO; 500, 1000 o 2000 */
CtrlResult  Ctrl_Pause(uint32_t now);           /* ya en PAUSA: OK y no cambia nada */
CtrlResult  Ctrl_Resume(uint32_t now);          /* fuera de PAUSA: CTRL_ERR_STATE */

/* Accion de una pulsacion ya validada. Devuelve true si cambio el estado. */
bool        Ctrl_OnButton(ButtonEvent ev, uint32_t now);

/* Solo para DIAG WRAP: el contador de tiempo salto; reinicia las marcas de tiempo. */
void        Ctrl_Rebase(uint32_t now);

void        Ctrl_GetInfo(CtrlInfo *out);
const char *Ctrl_ModeName(CtrlMode m);          /* "MANUAL", "AUTO" o "PAUSA" */

#endif /* CTRL_H */
