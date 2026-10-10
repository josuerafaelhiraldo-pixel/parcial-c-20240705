/* pwm_meter.h - Medidor INTERNO del PWM con TIM2 (modo "entrada PWM").
 *
 * Conexion: puente de PA6 (salida PWM, TIM3_CH1) a PA0 (TIM2_CH1, Arduino A0).
 *
 * Funcionamiento (todo en hardware, sin interrupciones):
 *   - TI1 = PA0. El flanco de subida de TI1FP1 reinicia el contador de TIM2
 *     (modo esclavo "reset") y captura en CCR1: CCR1 = cuentas de UN PERIODO.
 *   - El flanco de bajada de TI1 (canal 2, entrada indirecta) captura en CCR2:
 *     CCR2 = cuentas del tiempo en ALTO.
 *   - TIM2 es de 32 bits y cuenta a la frecuencia del reloj del temporizador
 *     (PSC = 0): con 84 MHz, una cuenta dura 11,9 ns.
 *   - La medicion NO espera: Meter_Start() la arma y Meter_Service() se llama en
 *     cada vuelta de main hasta que devuelve true (ventana de APP_METER_WINDOW_MS).
 *   - Sin flancos (0 % o 100 %): se informa el nivel de PA0 (con pull-down).
 *
 * LIMITACION: TIM2 y TIM3 comparten el mismo reloj (HSI -> PLL). Esta medicion
 * comprueba la logica y los registros (preescalador, ARR, CCR, niveles 0/100 %),
 * pero NO la exactitud absoluta del oscilador, y no sustituye a un instrumento
 * independiente. */
#ifndef PWM_METER_H
#define PWM_METER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum { METER_IDLE = 0, METER_RUNNING, METER_DONE } MeterState;

typedef struct {
    bool     hasEdges;       /* true: se midieron periodos; false: nivel constante */
    bool     levelHigh;      /* con hasEdges == false: nivel de PA0 al terminar */
    uint32_t samples;        /* periodos usados en los promedios */
    uint32_t captures;       /* capturas vistas en la ventana (incluye las descartadas) */
    uint32_t missed;         /* capturas perdidas (bandera de sobrecaptura) */
    uint32_t tickHz;         /* cuentas por segundo de TIM2 */
    uint32_t periodTicks;    /* periodo promedio, en cuentas */
    uint32_t freqMilliHz;    /* frecuencia medida, en milihertz */
    uint32_t dutyCentiPct;   /* duty medido, en centesimas de % (2500 = 25,00 %) */
} MeterResult;

typedef struct {
    bool    pass;            /* cumple todo */
    bool    constant;        /* se esperaba nivel constante (duty 0 o 100) */
    bool    levelOk;         /* nivel constante correcto y sin pulsos */
    bool    freqOk;          /* |error de frecuencia| <= 2 % */
    bool    dutyOk;          /* |error de duty| <= 2 puntos */
    int32_t freqErrCenti;    /* error de frecuencia en centesimas de % */
    int32_t dutyErrCenti;    /* error de duty en centesimas de punto */
} MeterVerdict;

void       Meter_Init(void);                 /* configura PA0 y TIM2 (independiente de la config. de CubeMX) */
void       Meter_Start(uint32_t now);        /* arma una medicion; now = Tick_Ms() */
bool       Meter_Service(uint32_t now);      /* true en la llamada en que termina la medicion */
MeterState Meter_State(void);
void       Meter_GetResult(MeterResult *out);

/* Compara una medicion con lo esperado: frecuencia en Hz, duty en %. */
bool       Meter_Judge(const MeterResult *r, uint32_t expFreqHz, uint32_t expDutyPct, MeterVerdict *v);

#endif /* PWM_METER_H */
