# Hito 4 - Integracion y comandos: resultados

- Fecha: 2026-10-10
- Placa: NUCLEO-F446RE (PWM en PA6 con TIM3 CH1; medidor interno con TIM2 CH1 en PA0, con el puente PA6-PA0 puesto)
- Herramientas: STM32CubeIDE 2.2.0, STM32CubeMX 6.18.1
- Terminal: Tera Term, COM8, 115200 baudios, 8N1, sin control de flujo, Local echo apagado (la placa repite lo que se escribe), Enter = CR
- Codigo probado: `ctrl.c`, `line.c`, `cmd.c`, `shell.c`, `app.c` y `console.c` del Hito 4 (commits "Hito 4: lectura de linea y analisis de ordenes (line, cmd) con pruebas", "Hito 4: maquina de estados MANUAL/AUTO/PAUSA y rampa (ctrl)" y "Hito 4: interprete de ordenes, telemetria e integracion (shell, app, consola)")
- Diseno: `analisis/estados_y_comandos.md`

## Que se integro en este hito

La aplicacion de pruebas de los Hitos 1 a 3 (teclas sueltas) se reemplaza por la aplicacion real:

- Maquina de estados MANUAL / AUTO / PAUSA (`ctrl.c`) con la rampa de AUTO (10 % a 90 %, 1 punto cada 20 ms) y los patrones del LED (fijo, 500 ms, 125 ms).
- Lectura de lineas (`line.c`), analisis de ordenes y validacion numerica (`cmd.c`) y ejecucion, respuestas y telemetria (`shell.c`).
- El pulsador B1 actua sobre la maquina de estados: corta = MANUAL <-> AUTO, larga = entrar o salir de PAUSA, corta en PAUSA = ignorada.

## Resultados en la placa

| ID | Prueba | Resultado esperado | Resultado observado | Estado | Evidencia |
|---|---|---|---|---|---|
| H4-01 | Arranque, `HELP` y `STATUS` | Banner del Hito 4, MANUAL, 1000 Hz, 25 %, STREAM OFF; `HELP` lista las ordenes y termina en `OK`; `STATUS` en una linea | Banner `... (Hito 4) ...`, `PWM: 1000 Hz, duty 25 %  (reloj TIM3=84000000 Hz, PSC=83, ARR=999, CCR=250)`, `Modo MANUAL, STREAM OFF.`. `STATUS`: `mode=MANUAL freq=1000 duty=25 manual=25 ramp=10 dir=up resume=- stream=OFF short=0 long=0 short_ign=0 cmd_ok=1 err_*=0 rx_lost=0 uart_err=0 tx_drop=0` | OK | `evidencias/hito4/01_boot_help_status.png` |
| H4-02 | Ordenes y errores (bloque pegado desde la consola, tras `RESET`) | Respuesta exacta a cada orden; ningun rechazo cambia parametros | Ver detalle abajo; `STATUS` final: `cmd_ok=5 err_cmd=1 err_arg=1 err_range=3 err_state=2 err_ovf=0` | OK | `evidencias/hito4/02_comandos_errores.png` |
| H4-02b | Primera orden pegada tras `RESET` (observacion) | - | En dos pegados distintos, el primer `DUTY 60` respondio `ERR COMMAND`; escrita a mano o pegada con una linea en blanco antes respondio `OK DUTY 60`. Ver "Observaciones" | Anotado | `evidencias/hito4/02b_primer_comando_con_byte_extra.png` |
| H4-03 | Boton B1 con `STREAM ON`: corta, larga, corta en PAUSA, larga, larga, `RESUME`, `STATUS` | Eventos `EVT` correctos; PAUSA con duty 0; la rampa continua desde el valor guardado | Ver detalle abajo; `STATUS` final: `short=2 long=3 short_ign=1 cmd_ok=2` | OK | `evidencias/hito4/03_boton_eventos.png` |
| H4-04 | Telemetria con `STREAM ON` | Una linea `TEL` cada 1000 ms | Dos corridas: `up=45335, 46335, ... 59335` y `up=89291, 90291, ... 102291`; todas las diferencias son de exactamente 1000 ms | OK | `evidencias/hito4/04_telemetria.png` |
| H4-05 | `DUTY 40`, `FREQ 500`, `DIAG MEASURE` (con el puente) | Medicion interna dentro de +-2 % y +-2 puntos | `OK DIAG MEASURE (TIM3: reloj 84000000 Hz, PSC=83, ARR=1999, CCR=800)` y `[medida] 500 Hz duty 40 % -> f=500.0 Hz (+0.00 %) duty=39.99 % (-0.01 pp) n=98 OK` | OK | `evidencias/hito4/05_diag_measure.png` |
| H4-06 | Lineas de mas de 63 caracteres y luego `STATUS` | `ERR OVERFLOW` por cada linea; la orden siguiente funciona | Dos lineas largas respondieron `ERR OVERFLOW`; el `STATUS` siguiente respondio normal con `err_ovf=2 rx_lost=0 uart_err=0` | OK | `evidencias/hito4/06_linea_larga.png` |
| H4-07 | LED LD2 y LED externo (PA6) durante H4-03 | LD2: fijo en MANUAL, 500 ms en AUTO, 125 ms en PAUSA. Externo: intensidad fija en MANUAL, variable en AUTO, apagado en PAUSA | LD2 se vio como se esperaba en cada estado. En el LED externo: intensidad fija en MANUAL; en AUTO la intensidad variaba (rampa); en PAUSA se apagaba; al volver de PAUSA (larga o `RESUME`) volvia a variar. `STATUS` no alteraba el LED. Observacion a simple vista | OK | Se mostrara en el video |

### Detalle de H4-02 (ordenes y errores)

| Orden enviada | Respuesta obtenida | Esperado |
|---|---|---|
| `DUTY 60` | `OK DUTY 60` | OK |
| `DUTY 20abc` | `ERR ARG` | ERR ARG |
| `DUTY -1` | `ERR RANGE` | ERR RANGE |
| `DUTY 101` | `ERR RANGE` | ERR RANGE |
| `FREQ 750` | `ERR RANGE` | ERR RANGE |
| `FREQ 2000` | `OK FREQ 2000` | OK |
| `MODE AUTO` | `OK MODE AUTO duty=10` | OK (la rampa empieza en 10 %) |
| `DUTY 50` (en AUTO) | `ERR STATE` | ERR STATE |
| `PAUSE` | `OK PAUSE` | OK |
| `MODE MANUAL` (en PAUSA) | `ERR STATE` | ERR STATE |
| `RESUME` | `OK RESUME mode=AUTO duty=11` | OK |
| `status` (minuscula) | `ERR COMMAND` | ERR COMMAND |
| `STATUS` | `OK mode=AUTO freq=2000 duty=24 manual=60 ramp=24 dir=up resume=- ... cmd_ok=5 err_cmd=1 err_arg=1 err_range=3 err_state=2 ...` | Contadores acordes |

Las respuestas de error no cambiaron ningun parametro: el `STATUS` final conserva `freq=2000` y `manual=60` (el duty manual guardado se mantuvo mientras el modo era AUTO).

### Detalle de H4-03 (boton B1 y PAUSA)

| Accion | Linea observada | Comentario |
|---|---|---|
| Corta en MANUAL | `EVT B1 short ok mode=AUTO duty=10 n_short=1 n_long=0` | AUTO empieza en 10 % |
| (sin accion) | `TEL ... mode=AUTO duty=20`, `duty=70`, `duty=61` | Rampa 1 punto por 20 ms: sube hasta 90, baja |
| Larga | `EVT B1 long ok mode=PAUSA duty=0 n_short=1 n_long=1` | Una sola larga; PWM en 0 % |
| (en PAUSA) | `TEL ... mode=PAUSA freq=1000 duty=0 manual=25` | La telemetria sigue; duty manual y frecuencia intactos |
| Corta en PAUSA | `EVT B1 short ignored mode=PAUSA duty=0 n_short=2 n_long=1` | Ignorada: no cambia de modo |
| Larga | `EVT B1 long ok mode=AUTO duty=14 n_short=2 n_long=2` | Sale de PAUSA a AUTO con el valor guardado (14) |
| Larga | `EVT B1 long ok mode=PAUSA duty=0 n_short=2 n_long=3` | Segunda pausa |
| `RESUME` por consola | `OK RESUME mode=AUTO duty=14` | Continua desde el valor guardado |
| `STATUS` | `... mode=AUTO duty=28 ramp=28 dir=down ... short=2 long=3 short_ign=1 cmd_ok=2 ...` | `cmd_ok=2` (STREAM ON y RESUME); no se cuenta el `STATUS` en curso |

Comprobaciones hechas con las lineas de `TEL`: al reanudar, el valor de la rampa continua desde el que tenia al pausar y la rampa sigue en la direccion guardada; no se recuperan los pasos del tiempo en pausa. `short=2` cuenta las dos cortas detectadas; `short_ign=1` indica cuantas de ellas se ignoraron (la de PAUSA).

## Observaciones

- **Primera orden pegada tras RESET (H4-02b).** Al pegar el bloque de ordenes justo despues de un RESET, el primer `DUTY 60` respondio `ERR COMMAND` (en dos intentos); el resto del bloque respondio bien. Al escribirla a mano, o al pegar con una linea vacia antes, respondio `OK DUTY 60`. En el simulador, pegar el mismo bloque a la velocidad real del puerto, con CR o con CRLF, funciona; el sintoma exacto aparece solo si antes del primer caracter llega un byte no ASCII (por ejemplo, los tres bytes de una marca BOM), que el eco no muestra. La causa mas probable es un byte invisible del texto copiado; no esta confirmada. El comportamiento del firmware coincide con lo pedido: una linea con caracteres no validos no se ejecuta.
- **Contador de cortas.** `short` cuenta las pulsaciones cortas detectadas, incluida la que se ignora en PAUSA; `short_ign` cuenta solo las ignoradas.
- **LED a simple vista.** El patron del LED de la placa y el brillo del LED externo se verificaron observandolos; no hay medicion de los 500 ms ni de los 125 ms. Los tiempos de la rampa y de los parpadeos se verifican con precision en las pruebas de PC (ver abajo) y se filmaran en el video.

## Pruebas de logica en PC (`pruebas/host/`)

`sh pruebas/host/run_tests.sh` (requiere `gcc`) compila con `-Werror`, ASan y UBSan y ejecuta 12 conjuntos de pruebas con un modelo del hardware (`hal_sim.c`): ringbuf, consola, tick, boton, PWM, medidor, verificacion, y los nuevos de este hito:

- `test_line.c`: CR, LF y CRLF (incluso separados en el tiempo), 63 y 64 caracteres, 5000 caracteres sin terminador, retroceso, caracteres invalidos, `Line_Poison` y 2 millones de bytes al azar sin salirse de los limites.
- `test_cmd.c`: mas de 100 ordenes validas e invalidas con su error exacto (incluidos `20abc`, `-1`, `101`, `4294967396` y numeros mayores que 64 bits), espacios y tabuladores, 300 000 lineas al azar.
- `test_ctrl.c`: rampa milisegundo a milisegundo durante dos ciclos (tramo de 1,6 s y ciclo de 3,2 s), LED a 500 y 125 ms, PAUSA desde AUTO subiendo y bajando, restauracion del duty manual, sin recuperar pasos al reanudar, `PAUSE` repetido, accion de B1 en cada estado, retraso de `main` y desbordamiento de uint32.
- `test_protocol.c`: respuestas exactas de todas las ordenes y errores, CR/LF/CRLF, 63/64/100 caracteres, perdida de bytes en RX y errores de hardware, contrapresion de la cola TX, telemetria, `DIAG`.
- `test_app.c`: B1 + maquina de estados + consola juntos: AUTO durante 3,2 s, 20 cortas con rebotes, larga de 3 s, PAUSA y `RESUME`, 10 ordenes por segundo durante 30 s con `STREAM ON`, tiempos de respuesta, desbordamiento de uint32.

Para comprobar que las pruebas detectan fallos reales se inyectaron 110 errores a proposito en `ctrl.c`, `line.c`, `cmd.c`, `shell.c`, `app.c` y `console.c`; las pruebas detectaron los 110 (la primera corrida dejo 9 sin detectar y se agregaron pruebas para cubrirlos). Estas pruebas verifican la **logica**; no sustituyen a las de la placa.

## Limitaciones de este hito

- **Pruebas de aceptacion incompletas.** En este hito se verificaron en la placa el arranque, las ordenes y errores principales, el boton con PAUSA, la telemetria y las lineas largas. Faltan, con registros completos, las pruebas de aceptacion de CR/LF/CRLF, 100 caracteres exactos, 10 ordenes por segundo durante 30 s, desbordamiento de RX provocado y tiempo cerca de UINT32_MAX (Hito 5; la orden `DIAG` esta para provocarlas).
- **Perdida de bytes en RX.** Si se detecta un byte perdido o danado, se vacia el buffer y la linea en curso se descarta hasta su terminador (`ERR OVERFLOW`); puede perderse alguna orden valida que esperaba en el buffer, pero nunca se ejecuta un fragmento (ver `analisis/estados_y_comandos.md`).
- **`DIAG` no es parte del protocolo** del enunciado; se puede desactivar con `APP_DIAG_ENABLED` en `app_config.h`.
- **Medicion interna del PWM** (ver `analisis/pwm_y_medicion.md`): comparte reloj con el PWM y no es un instrumento independiente.
