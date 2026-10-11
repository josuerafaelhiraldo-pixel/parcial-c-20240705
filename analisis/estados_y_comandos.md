# Analisis: maquina de estados, interprete de ordenes y telemetria (Hito 4)

Aplica a `Src/ctrl.c`, `Src/line.c`, `Src/cmd.c`, `Src/shell.c`, `Src/app.c`, `Src/console.c` e `Inc/app_config.h`.

## 1. Maquina de estados (`ctrl.c`)

| Estado | PWM | LD2 (PA5) | Sale por |
|---|---|---|---|
| MANUAL (inicial) | 1000 Hz, ultimo duty manual (25 % al arrancar) | Encendido fijo | Corta -> AUTO; larga o `PAUSE` -> PAUSA |
| AUTO | Rampa triangular 10 % <-> 90 %, 1 punto cada 20 ms | Alterna cada 500 ms | Corta -> MANUAL; larga o `PAUSE` -> PAUSA |
| PAUSA | 0 % (salida constante en bajo) | Alterna cada 125 ms | Larga o `RESUME` -> estado previo; la corta se ignora |

### Rampa de AUTO

- Al entrar (por B1 o por `MODE AUTO`) la rampa vale 10 % y asciende. `MODE AUTO` estando ya en AUTO la reinicia en 10 %.
- Cada 20 ms: `+1` (o `-1`). En 90 % cambia a descenso y en 10 % a ascenso.
- 10 -> 90 son 80 pasos = 80 x 20 ms = **1,6 s**; el ciclo completo son 160 pasos = **3,2 s** y vuelve a 10 %. Las pruebas en PC comparan el duty de cada milisegundo de dos ciclos completos con la formula del triangulo.
- El siguiente instante de paso se calcula sumando el periodo (`s_rampLast += 20`), no copiando la hora actual: asi un retraso de `main` no se acumula como deriva. Si `main` se retrasa varios pasos, se recuperan de uno en uno en las vueltas siguientes.

### PAUSA

- `PAUSE` / pulsacion larga guarda el estado previo (`s_resumeTo`), pone el PWM al 0 % y **no toca** el duty manual, el valor de la rampa, su direccion ni la frecuencia.
- Estando ya en PAUSA, `PAUSE` responde `OK` y no cambia nada (no se sobrescribe `s_resumeTo`).
- `RESUME` / larga restaura el duty del estado previo (manual o valor guardado de la rampa) y **reinicia los temporizadores** de la rampa y del LED a la hora actual: el primer paso llega 20 ms despues de reanudar y no se recuperan pasos del tiempo pausado. Las pruebas comprueban: pausa de 1 s, reanudar, 19 ms sin cambio, a los 20 ms un solo paso, en la misma direccion (subiendo o bajando).
- Volver a MANUAL (`MODE MANUAL` o corta desde AUTO) restaura el **ultimo duty manual**, que `DUTY n` actualiza solo en MANUAL y que AUTO no modifica.

### Quien puede cambiar que

| Orden / evento | MANUAL | AUTO | PAUSA |
|---|---|---|---|
| `MODE MANUAL` | OK (restaura duty manual) | OK | `ERR STATE` |
| `MODE AUTO` | OK (desde 10 %) | OK (reinicia en 10 %) | `ERR STATE` |
| `DUTY n` | OK | `ERR STATE` | `ERR STATE` |
| `FREQ n` | OK | OK (conserva el duty de la rampa) | `ERR STATE` |
| `PAUSE` | OK | OK | OK (sin cambios) |
| `RESUME` | `ERR STATE` | `ERR STATE` | OK |
| `HELP`, `STATUS`, `STREAM ON/OFF` | OK | OK | OK |
| B1 corta | -> AUTO | -> MANUAL | se ignora (se cuenta en `short_ign`) |
| B1 larga | -> PAUSA | -> PAUSA | -> estado previo |

### Tiempo

`Ctrl_Service(now)` se llama en cada vuelta de `main`. Toda comparacion de tiempo es `(uint32_t)(ahora - antes) >= limite`; nunca se reinicia el contador de milisegundos. Ninguna ISR toca este modulo: las ISR de B1, TIM6 y USART2 solo anotan datos.

## 2. Interprete de ordenes

### Camino de un byte

```
USART2 RX (ISR) -> buffer circular de 256 B (255 utiles; el enunciado pide >= 128)
   -> main: Shell_Service (como mucho 32 bytes y UNA orden por vuelta)
   -> line.c: arma la linea (CR, LF o CRLF; hasta 63 caracteres + NUL)
   -> cmd.c : analiza la orden y valida los argumentos
   -> shell.c: comprueba el estado y ejecuta (ctrl.c), responde por la cola TX
```

La ISR de RX solo mete el byte en el buffer circular y cuenta errores. El analisis, las respuestas y la telemetria se hacen en `main`.

### Lineas (`line.c`)

- Terminadores CR, LF y CRLF. Un LF justo despues de un CR se ignora, asi que CRLF es **una sola** orden. Una linea vacia o solo con espacios no es orden (no hay respuesta ni error).
- Maximo 63 caracteres (+ NUL). El caracter 64 marca la linea como demasiado larga: se descarta **todo** hasta el siguiente terminador, sin ejecutar ningun fragmento, y ese terminador produce `ERR OVERFLOW`. La linea siguiente empieza limpia.
- Retroceso (BS o DEL) borra el ultimo caracter (si la linea no esta ya descartada). NUL, otros controles y bytes >= 0x80 invalidan la linea -> `ERR COMMAND` (un NUL no puede truncar la orden).
- El tabulador cuenta como espacio. Espacios al inicio y al final se aceptan.

### Ordenes (`cmd.c`)

- Ordenes y palabras en MAYUSCULAS (`status` -> `ERR COMMAND`).
- Sin argumentos sobrantes ni faltantes: `HELP me`, `DUTY`, `DUTY 1 2`, `MODE` -> `ERR ARG`.
- Los numeros se validan completos, sin `atoi`: solo digitos. `DUTY 20abc`, `5.5`, `+5`, `0x10`, `1e2` -> `ERR ARG`. Un signo menos seguido de digitos es un entero negativo -> `ERR RANGE` (`DUTY -1`). Una cadena de digitos que no cabe en 32 bits -> `ERR RANGE` (se detecta el desbordamiento al acumular; `4294967396` = 2^32 + 100 no da 100). Ceros a la izquierda (`007`) son digitos validos.
- `DUTY n`: 0 a 100. `FREQ n`: solo 500, 1000 o 2000; otro numero bien formado -> `ERR RANGE`.

### Orden de las comprobaciones

1. Orden desconocida -> `ERR COMMAND`.
2. Argumentos mal formados, faltantes o sobrantes -> `ERR ARG`.
3. Valor fuera de rango -> `ERR RANGE`.
4. Si todo es valido, el estado actual debe permitirla -> si no, `ERR STATE`.

Es una decision de diseno: primero se valida la orden y sus argumentos (no dependen del estado) y despues se mira si el estado la permite. Un rechazo nunca cambia parametros (las pruebas comprueban duty, frecuencia, modo y duty manual despues de cada error).

### Respuestas

| Orden | Respuesta |
|---|---|
| `HELP` | Lista de ordenes y, al final, una linea `OK` |
| `STATUS` | Una linea: `OK mode=... freq=... duty=... manual=... ramp=... dir=... resume=... uptime_ms=... stream=... short=... long=... short_ign=... cmd_ok=... err_cmd=... err_arg=... err_range=... err_state=... err_ovf=... rx_lost=... uart_err=... tx_drop=...` |
| `MODE MANUAL` / `MODE AUTO` | `OK MODE MANUAL duty=25` / `OK MODE AUTO duty=10` |
| `DUTY n`, `FREQ n` | `OK DUTY n`, `OK FREQ n` |
| `PAUSE`, `RESUME` | `OK PAUSE`, `OK RESUME mode=AUTO duty=47` |
| `STREAM ON` / `OFF` | `OK STREAM ON` / `OK STREAM OFF` |
| Error | `ERR COMMAND`, `ERR ARG`, `ERR RANGE`, `ERR STATE` o `ERR OVERFLOW` |

`mode` es `MANUAL`, `AUTO` o `PAUSA`. `duty` es el duty aplicado (0 en PAUSA), `manual` el duty manual guardado, `ramp`/`dir` el valor y sentido de la rampa (se conservan en PAUSA), `resume` el estado al que vuelve `RESUME` (`-` fuera de PAUSA). `cmd_ok` y los contadores `err_*` son los acumulados **antes** de esta orden. `err_ovf` cuenta lineas descartadas (demasiado largas o con RX danada); `rx_lost` son bytes perdidos por buffer lleno; `uart_err` suma ORE+FE+NE+PE; `tx_drop` son mensajes descartados por la cola TX llena.

La placa repite (eco) lo que se escribe; con `APP_CONSOLE_ECHO 0` no hay eco.

### Telemetria y eventos

- `STREAM ON`: una linea `TEL up=<ms> mode=... freq=... duty=... manual=... short=... long=...` cada 1000 ms. Inicialmente desactivada; sigue activa en PAUSA. `STREAM ON` repetido no reinicia la cuenta.
- La hora de la siguiente linea avanza sumando 1000 ms (sin deriva). Si la cola TX no tiene espacio, la linea se reintenta en la vuelta siguiente (no se pierde); si `main` se atraso mas de un periodo, no se manda una rafaga.
- Con `STREAM ON` tambien sale una linea `EVT B1 short|long ok|ignored mode=... duty=... n_short=... n_long=...` por cada pulsacion validada. Con `STREAM OFF` no sale nada sin que se pida.

### Salida sin bloquear (contrapresion)

Todas las respuestas pasan por `Console_WriteBytes`, que nunca espera. `shell.c` comprueba `Console_TxFree()` y solo escribe la respuesta si cabe **entera** (cola de 1024 B; `HELP` ocupa unos 920 B). Si no cabe, queda pendiente y **no se lee la orden siguiente** hasta enviarla: los bytes esperan en el buffer circular de RX. Las respuestas no se cortan ni se pierden. El eco y las lineas EVT son de mejor esfuerzo: si no caben se descartan y `tx_drop` lo cuenta.

### Perdida de bytes en RX

`Console_RxFaultCount()` suma lo que puede haber perdido o danado bytes: buffer de RX lleno, overrun (ORE), trama (FE), ruido (NE) y paridad (PE). Si ese valor cambia, `shell.c` **vacia el buffer de RX** y marca la linea en curso como danada: se ignora hasta su terminador y se responde `ERR OVERFLOW` (queda contado en `err_ovf`). A lo sumo se pierde alguna orden valida que esperaba en el buffer; **nunca se ejecuta un fragmento**. Si el terminador de la linea danada tambien se perdio, el siguiente fin de linea (la orden siguiente) cierra la descartada y la consola recupera en la linea posterior. Una orden valida que llega despues, ya con la linea limpia, funciona.

### Orden `DIAG` (diagnostico, no es parte del protocolo)

Existe porque el enunciado pide provocar y documentar situaciones dificiles de producir a mano. Se desactiva con `APP_DIAG_ENABLED 0` (entonces `DIAG` responde `ERR COMMAND`).

| Orden | Para que sirve |
|---|---|
| `DIAG MEASURE` | Mide con TIM2 la salida actual (solo en MANUAL; requiere el puente PA6-PA0) y la compara con lo programado |
| `DIAG PWMALL` | Autoprueba de 15 combinaciones de frecuencia y duty (solo en MANUAL) |
| `DIAG STALL n` | `main` deja de leer RX durante `n` ms (100 a 5000): provoca el desbordamiento del buffer de RX si se envian mas de 255 bytes en ese tiempo |
| `DIAG WRAP` | Pone el contador de tiempo a 2048 ms de UINT32_MAX: la rampa, el LED, la telemetria y B1 cruzan el desbordamiento |

Mientras corre una verificacion del PWM, las ordenes que cambian el estado (`MODE`, `DUTY`, `FREQ`, `PAUSE`, `RESUME`, `DIAG`) responden `ERR STATE` y B1 no cambia el estado (se informa como `ignored`), para que la medicion no se mezcle con otros cambios.

## 3. Reparto de trabajo entre ISR y main

| Contexto | Que hace | Que escribe |
|---|---|---|
| ISR EXTI13 (B1) | Cuenta el flanco | `s_isrEdges` |
| ISR TIM6 (1 kHz) | Incrementa el contador de milisegundos | `s_ms` |
| ISR USART2 (RX) | Guarda el byte en el buffer circular; cuenta errores | buffer RX, contadores |
| ISR USART2 (TX) | Libera lo enviado y arranca el siguiente tramo | indice de la cola TX |
| `main` | Todo lo demas: antirrebote, FSM, rampa, LED, parser, respuestas, telemetria | resto del estado |

Ninguna ISR llama a `Console_Printf`, al parser ni a la maquina de estados.

## 4. Tiempo y desbordamiento

Comparaciones de tiempo del Hito 4, todas con resta sin signo: rampa (`s_rampLast`), LED (`s_ledLast`), telemetria (`s_telLast`), espera de `DIAG STALL`, verificacion del PWM y B1. `DIAG WRAP` mueve el contador a `0xFFFFF800`; las marcas de tiempo de la FSM y de la telemetria se reinician a la hora nueva (sin eso la resta daria un salto enorme y habria una rafaga de pasos). Las pruebas en PC cruzan UINT32_MAX con la rampa, el LED, la telemetria, la espera de `DIAG STALL` y B1.

`uptime_ms` es `Tick_Ms()` y da la vuelta cada 2^32 ms (49,7 dias).

## 5. Memoria

| Buffer | Tamano | Nota |
|---|---|---|
| RX circular | 256 B (255 utiles) | Enunciado: >= 128 |
| TX circular | 1024 B | `HELP` (~920 B) cabe entero |
| Linea de orden | 64 B | 63 caracteres + NUL |
| Respuesta formateada | 384 B | `STATUS` ocupa ~270 B |
| `Console_Printf` | 256 B (pila) | Solo se usa desde `main` |

## 6. Pruebas en PC (`pruebas/host/`)

| Archivo | Que comprueba |
|---|---|
| `test_line.c` | CR, LF, CRLF (incluso separados en el tiempo), 63 y 64 caracteres, linea larga sin terminador (5000 caracteres), retroceso, caracteres invalidos, `Line_Poison`, 2 millones de bytes al azar sin salirse de los limites |
| `test_cmd.c` | Mas de 100 ordenes validas e invalidas con su error exacto, numeros (`20abc`, `-1`, `101`, `4294967396`, 2^64 + 100...), espacios y tabuladores, 300 000 lineas al azar |
| `test_ctrl.c` | Rampa milisegundo a milisegundo durante dos ciclos, tramos de 1,6 s, LED a 500 y 125 ms, restauracion del duty manual, PAUSA desde AUTO subiendo y bajando, sin recuperar pasos, `PAUSE` repetido, accion de B1, retraso de `main`, desbordamiento de uint32 |
| `test_protocol.c` | Respuestas exactas de todas las ordenes, 24 errores, estados, CR/LF/CRLF, 63/64/100 caracteres, retroceso, `STREAM`, perdida de bytes en RX y errores de hardware, contrapresion de la cola TX, `DIAG`, `DIAG WRAP`, telemetria atrasada |
| `test_app.c` | B1 + FSM + consola juntos: AUTO 3,2 s, 20 cortas con rebotes, larga de 3 s, PAUSA/RESUME, 10 ordenes por segundo durante 30 s con la rampa y B1 activos, tiempos de respuesta, desbordamiento de uint32 |

Con errores inyectados a proposito en `ctrl.c`, `line.c`, `cmd.c`, `shell.c`, `app.c` y `console.c`, las pruebas detectaron todos (ver `pruebas/hito4_resultados.md`). Estas pruebas verifican la **logica** con un modelo del HAL; no sustituyen a las de la placa.

## 7. Limitaciones y decisiones

- **Orden de errores**: sintaxis (`ERR ARG`), rango (`ERR RANGE`), y despues estado (`ERR STATE`). Por ejemplo `DUTY 200` en AUTO responde `ERR RANGE`, y `DUTY 50` en AUTO responde `ERR STATE`.
- **Numeros negativos**: `DUTY -1` y `FREQ -1000` responden `ERR RANGE` (son enteros fuera de rango), no `ERR ARG`.
- **Perdida de bytes en RX**: se descarta lo que hay en el buffer y la linea afectada; puede perderse una orden valida que ya esperaba. Nunca se ejecuta un fragmento.
- **La linea larga responde al llegar el terminador**: si se envian mas de 63 caracteres sin pulsar Enter no hay respuesta hasta el terminador.
- **Eco y lineas EVT**: de mejor esfuerzo (se descartan y se cuentan si la cola TX esta llena).
- **`DIAG`**: no forma parte del protocolo del enunciado; se puede desactivar en `app_config.h`.
- **Medicion del PWM**: sigue siendo interna y comparte reloj con el PWM (ver `analisis/pwm_y_medicion.md`).

## 8. Codigo propio y codigo del SDK

| Archivo | Origen |
|---|---|
| `ctrl.c/.h`, `line.c/.h`, `cmd.c/.h`, `shell.c/.h` | Propio (sin HAL, salvo `ctrl.c` que usa `HAL_GPIO_WritePin` para LD2) |
| `app.c`, `console.c` | Propio; `console.c` usa `HAL_UART_Transmit_IT` y `HAL_UART_Receive_IT` del HAL |
| `main.c`, `usart.c`, `tim.c`, `gpio.c`, `stm32f4xx_it.c`, `Drivers/` | Generado por CubeMX / HAL |
