# Hito 5 - Pruebas de aceptacion y cierre: resultados

- Fecha: 2026-10-11
- Placa: NUCLEO-F446RE (PWM en PA6 con TIM3 CH1; medidor interno con TIM2 CH1 en PA0, con el puente PA6-PA0 puesto)
- Herramientas: STM32CubeIDE 2.2.0, STM32CubeMX 6.18.1, Tera Term (COM8, 115200 8N1, sin control de flujo, Local echo apagado, Enter = CR)
- Firmware probado: el del Hito 4, **sin cambios**. Las 13 pruebas de aceptacion pasaron sin tener que corregir el codigo.
- Registros: `evidencias/hito5/prueba_00_humo.log` ... `prueba_08_boton.log`; salida del analizador: `evidencias/hito5/analisis_logs.txt`
- Herramientas de prueba: `pruebas/herramientas/` (macros de Tera Term `.ttl`, `analizar_logs.py` y `LEEME.md`)

## Como se hicieron las pruebas

1. Se pulso RESET antes de cada prueba, de modo que todas parten de contadores en cero.
2. Las pruebas que necesitan bytes exactos o un ritmo fijo (CR/LF/CRLF, lineas largas, 10 ordenes por segundo, desbordamiento de RX) las ejecuta una macro de Tera Term. La macro envia las ordenes por COM8 y guarda todo lo que se ve en la consola (eco, respuestas, telemetria y eventos) en un archivo `.log`.
3. Las pulsaciones de B1 se hicieron a mano (una macro no puede pulsar el boton): 20 cortas y una larga de unos 3 s con `STREAM ON`, registradas en `prueba_08_boton.log`. En la macro 05 se pulso B1 unas 6 veces mientras corria la carga.
4. `analizar_logs.py` (Python 3, solo biblioteca estandar) lee cada registro y comprueba respuesta por respuesta, contadores, separacion de la telemetria y la rampa de AUTO contra el modelo esperado. Resultado con los 9 registros: **124 de 124 comprobaciones correctas**.
5. Los LED se observaron a simple vista durante las pruebas y se mostraran en el video; la observacion visual no se usa para verificar frecuencia ni duty (eso lo hace la medicion interna).

| Registro | Comprobaciones |
|---|---|
| `prueba_00_humo.log` | 3/3 |
| `prueba_01_duty_freq.log` | 20/20 |
| `prueba_02_auto_pausa.log` | 17/17 |
| `prueba_03_errores_y_lineas.log` | 35/35 |
| `prueba_04_terminadores.log` | 7/7 |
| `prueba_05_carga_10_por_segundo.log` | 12/12 |
| `prueba_06_overflow_rx.log` | 6/6 |
| `prueba_07_desbordamiento_uint32.log` | 10/10 |
| `prueba_08_boton.log` | 14/14 |

## Pruebas de aceptacion del enunciado

| # | Prueba | Resultado esperado | Resultado observado | Estado | Evidencia |
|---|---|---|---|---|---|
| 1 | Encendido | MANUAL, 1000 Hz, 25 %, STREAM OFF | Tras RESET, el primer `STATUS` da `mode=MANUAL freq=1000 duty=25 manual=25 stream=OFF short=0 long=0` | OK | `prueba_00_humo.log`, `prueba_01_duty_freq.log` |
| 2 | `DUTY 0 / 50 / 100` | Nivel bajo / 50 % / nivel alto | Medicion interna: 0 % -> sin flancos, nivel BAJO; 50 % -> duty 50,00 % (+0,00 pp, n=198); 100 % -> sin flancos, nivel ALTO. `STATUS` confirma `duty=0`, `50` y `100` | OK | `prueba_01_duty_freq.log` |
| 3 | `FREQ 500 / 1000 / 2000` | Error de frecuencia <= 2 %; el tick sigue en 1 ms | f = 500,0 / 1000,0 / 2000,0 Hz, error +0,00 % en las tres, duty 50,00 % en las tres. Las lineas `TEL` siguieron separadas exactamente 1000 ms durante los cambios de frecuencia (el tick no se altera) | OK | `prueba_01_duty_freq.log` |
| 4 | AUTO durante 3,2 s | Regresa a 10 % tras subir a 90 % y bajar | `MODE AUTO` responde `duty=10`. Con unos 15 s en AUTO (mas de 4 ciclos), el duty de cada `TEL` coincide con el triangulo 10-90-10 de ciclo 3,2 s (diferencia maxima 1 punto); la fase de la rampa no se desvia entre muestras (error maximo 17 ms, menos que un paso de 20 ms) y el valor se mantuvo siempre entre 10 y 90 % | OK | `prueba_02_auto_pausa.log` |
| 5 | 20 pulsaciones cortas separadas | 20 eventos, sin duplicados | 20 `EVT B1 short ok` con `n_short` de 1 a 20 sin saltos; MANUAL y AUTO se alternan (AUTO entra con 10 %); `short_ign=0`. `STATUS` final: `short=20` | OK | `prueba_08_boton.log` |
| 6 | Mantener el boton 3 s | Una larga; al soltar no genera corta | Un solo `EVT B1 long ok mode=PAUSA duty=0 n_short=20 n_long=1`; despues de soltar no aparece ninguna corta (`n_short` sigue en 20). `STATUS` final: `short=20 long=1` | OK | `prueba_08_boton.log` |
| 7 | PAUSA desde AUTO y RESUME | Duty 0 en pausa; continua desde el valor y la direccion guardados | `PAUSE` -> `mode=PAUSA duty=0`; 2,5 s despues, `ramp=16 dir=down` sin cambios. `RESUME` responde `OK RESUME mode=AUTO duty=16` y la rampa continua con el mismo ritmo (diferencia 0 puntos), sin recuperar los pasos del tiempo en pausa. `MODE MANUAL` restaura el duty manual (25 %) | OK | `prueba_02_auto_pausa.log` |
| 8 | `DUTY 20abc` / `-1` / `101` | ERR, sin modificar el duty | `DUTY 20abc` -> `ERR ARG`; `DUTY -1` -> `ERR RANGE`; `DUTY 101` -> `ERR RANGE`; ademas `DUTY` sin argumento, `DUTY 5 6`, `DUTY 99999999999999999999` y `DUTY 4294967396` fueron rechazados. Con 19 ordenes invalidas el duty siguio en 40. Contadores finales `err_cmd=2 err_arg=10 err_range=5 err_state=1` | OK | `prueba_03_errores_y_lineas.log` |
| 9 | Linea de 100 caracteres y luego `STATUS` | `ERR OVERFLOW`; `STATUS` posterior funciona | 100 caracteres `X` -> `ERR OVERFLOW` y el `STATUS` siguiente respondio normal. Limite exacto: `STATUS` con espacios hasta 63 caracteres -> `OK`; hasta 64 -> `ERR OVERFLOW`. `err_ovf=2` | OK | `prueba_03_errores_y_lineas.log` |
| 10 | CR, LF y CRLF | Una ejecucion por orden | 18 ordenes enviadas con CR, con LF, con CRLF, solas y en rafaga: 20 respuestas para 20 ordenes (incluidos 2 `STATUS`), cada una en su orden; `cmd_ok` subio exactamente en las 18 ordenes; sin errores. CRLF no produjo una orden extra | OK | `prueba_04_terminadores.log` |
| 11 | `STREAM ON` y 10 ordenes por segundo durante 30 s | Sin reinicios ni perdidas; rampa y boton operativos | 300 ordenes de lazo (100 `STATUS`, 100 `FREQ 500`, 100 `FREQ 2000`) mas `STREAM ON`, `MODE AUTO` y los `STATUS` de control: `cmd_ok=305`, `err_*=0`, `rx_lost=0`, `uart_err=0`, `tx_drop=0`, sin reinicio (el uptime siempre crecio). Ritmo logrado: **8,1 ordenes/s** (300 ordenes en 37,1 s; ver limitaciones). La rampa siguio activa (45 valores distintos en los `STATUS`), 40 lineas `TEL` separadas exactamente 1000 ms y las 6 pulsaciones de B1 dieron 6 `EVT short ok` | OK, con reserva en el ritmo | `prueba_05_carga_10_por_segundo.log` |
| 12 | Desbordamiento de RX provocado | Error contabilizado y recuperacion en la siguiente linea | Con `DIAG STALL 3000` la placa deja de leer RX 3 s mientras la macro envia 488 bytes (buffer de 255): `ERR OVERFLOW` al cerrar la linea; `STATUS`: `err_ovf=1 rx_lost=233`. La orden siguiente `DUTY 44` se ejecuto (`OK DUTY 44`, `STATUS` confirma `duty=44`) y no se ejecuto ningun fragmento: el duty seguia en 25 % tras el desbordamiento | OK | `prueba_06_overflow_rx.log` |
| 13 | Tiempo cerca de UINT32_MAX | La temporizacion continua al desbordar; prueba inyectada documentada | `DIAG WRAP` inyecta el contador a 2048 ms del maximo (`tick=4294965248`). Las `TEL` pasan de `up=4294967248` a `up=952`; todas siguen separadas exactamente 1000 ms, tambien al dar la vuelta. En AUTO, la rampa sigue a 20 ms por punto tras la vuelta (diferencia maxima 1 punto); `MODE MANUAL` restaura el duty manual | OK | `prueba_07_desbordamiento_uint32.log` |

Las pruebas 5, 6 y 13 tambien se hicieron en el Hito 2 con la aplicacion de prueba; aqui se repitieron con la aplicacion integrada, como pide el plan.

### Duty de 25 %, 50 % y 75 % con tolerancia de +-2 puntos

El enunciado pide demostrar 25, 50 y 75 %. La autoprueba del Hito 3 (`evidencias/hito3/06_autoprueba_15_de_15.png`, resultados en `pruebas/hito3_resultados.md`) midio las 15 combinaciones de 500, 1000 y 2000 Hz con 0, 25, 50, 75 y 100 %: 15/15 correctas, error de frecuencia +0,00 % y error de duty maximo de -0,01 puntos. En este hito se repitio por orden de consola el 50 % en las tres frecuencias (duty 50,00 %, +0,00 pp), el 0 % y el 100 %, ademas del 40 % del Hito 4 (39,99 %).

### Tiempos de respuesta (100 ms para las ordenes, 50 ms para B1)

- **No se midieron en la placa con un reloj externo**: el registro de Tera Term no lleva marcas de tiempo y no se dispone de instrumento para medir el intervalo entre el terminador y la primera respuesta.
- **Por diseno**: `main` atiende RX, el boton y la telemetria en cada vuelta y como maximo 32 bytes y una orden por vuelta; nunca espera. La respuesta se encola en la cola TX (1024 bytes) y la transmite la interrupcion de USART2. Un evento de B1 se confirma 30 ms despues del flanco (antirrebote) y su linea `EVT` se emite en la vuelta de `main` siguiente. La unica forma de superar el limite es que `main` se bloquee, y `DIAG STALL` es la unica espera, activada solo a peticion.
- **En el simulador de PC** (`pruebas/host/test_app.c`): `STATUS` responde en 2 ms o menos desde el terminador y la confirmacion de un cambio del boton ocurre 30-31 ms despues del flanco.
- **En la placa**: en las pruebas 04 y 05 cada orden obtuvo su respuesta antes de la siguiente (a 100 ms entre ordenes), sin respuestas perdidas ni desordenadas, lo que es coherente con el limite pero no lo mide.

## Observaciones

- **Ritmo de la prueba de carga.** La macro pide una orden cada 100 ms, pero `mpause` de Tera Term en Windows tiene una resolucion de unos 15 ms y cada `sendln` suma su propio retardo; el resultado fue una orden cada 123 ms (8,1 por segundo). La cantidad pedida (300 ordenes) se envio completa y sin un solo error; la carga exacta de 10 por segundo durante 30 s se cumple en las pruebas de PC (`test_app.c`: 300 ordenes en 30 s con `STREAM ON`, la rampa y el boton activos). La prueba se puede repetir bajando `mpause 100` a `mpause 75` en la macro 05.
- **Eco mezclado con la telemetria (prueba 08).** En el registro del boton aparecen lineas como `STTEL up=930378 ...` o `ATUTEL ...`. Es el eco de las letras de `STATUS`, `RESUME` y `STREAM OFF` escritas a mano mientras `STREAM ON` imprimia `TEL` cada segundo. Las tres ordenes se interpretaron bien (`OK mode=PAUSA ... short=20 long=1`, `OK RESUME mode=MANUAL duty=25`, `OK STREAM OFF`): el analizador separa estas lineas y la consola no pierde ni mezcla ordenes con la telemetria.
- **Primera orden tras un RESET (H4-02b).** En el Hito 4, el primer comando pegado desde el portapapeles respondio `ERR COMMAND` en dos ocasiones. En todas las pruebas de este hito, el primer comando enviado por cada una de las 8 macros tras el RESET (`STATUS` o `STREAM ON`) respondio bien. Es coherente con la explicacion de un byte invisible en el texto pegado, pero no la confirma.
- **Contador `short`.** `short` cuenta las cortas detectadas, incluidas las ignoradas en PAUSA (`short_ign`); en la prueba 08 no hubo ignoradas.
- **Ordenes `DIAG`.** Las pruebas 12 y 13 usan `DIAG STALL` y `DIAG WRAP` para provocar el desbordamiento de RX y el cruce de UINT32_MAX. No son parte del protocolo del enunciado y se desactivan con `APP_DIAG_ENABLED` en `app_config.h`.

## Pruebas de logica en PC

`sh pruebas/host/run_tests.sh` (requiere `gcc`) ejecuta 12 conjuntos de pruebas con ASan y UBSan sobre un modelo del hardware. No cambiaron en este hito (110 errores inyectados a proposito en el codigo, 110 detectados; ver `pruebas/hito4_resultados.md`). Estas pruebas verifican la logica y no sustituyen a las de la placa.

## Limitaciones

- **Medicion interna del PWM**: TIM2 mide la salida de TIM3 y ambos comparten el reloj. Valida registros, calculos y niveles, pero no es un instrumento independiente ni mide la exactitud absoluta del oscilador (HSI: +-1 % a 25 C segun el datasheet). El docente acepto por escrito esta medicion cuando no se dispone de osciloscopio (`evidencias/hito3/08_mensaje_al_profe_y_respuesta`). Detalle en `analisis/pwm_y_medicion.md`.
- **Tiempos de respuesta** no medidos con un reloj externo (ver arriba).
- **Ritmo de la prueba de carga** de 8,1 ordenes por segundo en la placa (ver Observaciones).
- **Perdida de bytes en RX**: si se pierde o daña un byte, se vacia el buffer y la linea en curso se descarta hasta su terminador (`ERR OVERFLOW`); puede perderse alguna orden valida que esperaba en el buffer, pero nunca se ejecuta un fragmento (`analisis/estados_y_comandos.md`).
- **Patrones de los LED** (fijo, 500 ms, 125 ms) comprobados a simple vista y con las pruebas de PC; se muestran en el video.

## Correcciones hechas en el Hito 5

Ninguna al firmware. Los cambios de este hito son las herramientas de prueba, los registros y la documentacion.
