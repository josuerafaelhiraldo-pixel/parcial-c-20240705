# Hito 3 - TIMER y PWM: resultados

- Fecha: 2026-10-10
- Placa: NUCLEO-F446RE (PWM en PA6 con TIM3 CH1; medidor interno con TIM2 CH1 en PA0)
- Herramientas: STM32CubeIDE 2.2.0, STM32CubeMX 6.18.1
- Terminal: Tera Term, COM8, 115200 baudios, 8N1, sin control de flujo
- Conexion: puente (cable Dupont) de PA6 (CN5 pin 5, D12) a PA0 (CN8 pin 1, A0). Foto: `evidencias/hito3/03_conexion_pa6_pa0.jpg`
- Codigo probado: `pwm.c`, `pwm_meter.c`, `pwm_check.c` y `app.c` del Hito 3 (commits "Agrego modulo PWM con TIM3: frecuencia, duty y niveles 0 y 100 %", "Agrego medidor interno del PWM con TIM2 en modo captura" y "Hito 3: autoprueba del PWM y teclas de control")
- Diseno y calculos: `analisis/pwm_y_medicion.md`

## Metodo de verificacion y aceptacion del docente

No se dispone de osciloscopio ni de analizador logico. El docente indico que esa medicion puede omitirse cuando no se dispone del instrumento. Aun asi, se verifica el PWM con una **medicion interna**: TIM2 en modo "entrada PWM" mide la salida de PA6 a traves de un puente. El metodo se le comunico al docente por escrito y respondio aceptandolo (`evidencias/hito3/08_mensaje_al_profe_y_respuesta`).

La medicion comprueba la logica y los registros (preescalador, ARR, CCR, niveles de 0 y 100 %, ausencia de pulsos, conexion), **pero no es un instrumento independiente**: TIM2 y TIM3 comparten el reloj. Detalle en "Limitaciones".

## Compilacion y carga

- Build (11:20): `0 errors, 0 warnings`, text 31 352 B, data 92 B, bss 4 280 B (`evidencias/hito3/01_compilacion_0_errores`).
- Carga con ST-LINK: 30,72 KB programados y `Download verified successfully`.
- TIM2 en CubeMX: Clock Source = Internal Clock, Combined Channels = PWM Input on CH1, Prescaler 0, Counter Period 4294967295, sin NVIC; PA0 = `TIM2_CH1` (`evidencias/hito3/02_cubemx_tim2_pwm_input`). `Meter_Init()` ademas escribe de forma explicita los registros de TIM2 y configura PA0 con pull-down.

## Resultados en la placa

| ID | Prueba | Resultado esperado | Resultado observado | Estado | Evidencia |
|---|---|---|---|---|---|
| H3-01 | Arranque tras RESET | PWM en 1 kHz y 25 %; registros acordes al reloj real | `PWM: 1000 Hz, duty 25 %  (reloj TIM3=84000000 Hz, PSC=83, ARR=999, CCR=250)` | OK | `evidencias/hito3/04_arranque.png` |
| H3-02 | Medida unica de la salida actual (tecla `p`) | Frecuencia con error <= 2 % y duty dentro de +-2 puntos | `f=1000.0 Hz (+0.00 %)`, `duty=24.99 % (-0.01 pp)`, `n=198` periodos, `OK` | OK | `evidencias/hito3/05_medida_unica_p.png` |
| H3-03 | Autoprueba con el puente (tecla `a`): 3 frecuencias x 5 duty | 15/15 combinaciones correctas | `Resultado: 15/15 OK` (detalle abajo) | OK | `evidencias/hito3/06_autoprueba_15_de_15.png` |
| H3-04 | Autoprueba sin el puente (PA0 suelto) | El medidor detecta la falta de senal; solo 0 % puede pasar | `Resultado: 3/15 OK`: pasan solo las tres de 0 % (lineas 1, 6 y 11); las otras 12 dan `sin flancos, nivel BAJO ... FALLA` | OK | `evidencias/hito3/07_puente_suelto_falla.png` |
| H3-05 | Teclas `f` y `d`, luego `s` | `f` cicla 500 -> 1000 -> 2000; `d` cicla 0 -> 25 -> 50 -> 75 -> 100; `s` muestra la linea PWM | `f`: 2000, 500, 1000 Hz con ARR 499, 1999, 999. `d` desde 25 %: 50 (CCR=500), 75 (CCR=750), 100 (CCR=1000), 0 (CCR=0), 25 (CCR=250). `s`: `overflow_buffer=0`, `mensajes_descartados=0`, `ORE/FE/NE/PE=0` | OK | `evidencias/hito3/09_teclas_f_d_s.png` |
| H3-06 | El tick de 1 ms no cambia al cambiar la frecuencia (15 cambios durante la autoprueba) | TIM6 y SysTick avanzan igual | `TIM6 +3300 ms, SysTick +3300 ms, diferencia 0 ms` | OK | `evidencias/hito3/06_autoprueba_15_de_15.png` |

### Detalle de H3-03 (autoprueba con el puente)

| # | Frecuencia | Duty pedido | Medido |
|---|---|---|---|
| 1 | 500 Hz | 0 % | sin flancos, nivel BAJO, capturas=0 |
| 2 | 500 Hz | 25 % | f = 500.0 Hz (+0,00 %), duty = 24,99 % (-0,01 pp), n = 98 |
| 3 | 500 Hz | 50 % | f = 500.0 Hz (+0,00 %), duty = 50,00 % (+0,00 pp), n = 98 |
| 4 | 500 Hz | 75 % | f = 500.0 Hz (+0,00 %), duty = 75,00 % (+0,00 pp), n = 98 |
| 5 | 500 Hz | 100 % | sin flancos, nivel ALTO, capturas=0 |
| 6 | 1000 Hz | 0 % | sin flancos, nivel BAJO, capturas=0 |
| 7 | 1000 Hz | 25 % | f = 1000.0 Hz (+0,00 %), duty = 24,99 % (-0,01 pp), n = 198 |
| 8 | 1000 Hz | 50 % | f = 1000.0 Hz (+0,00 %), duty = 50,00 % (+0,00 pp), n = 198 |
| 9 | 1000 Hz | 75 % | f = 1000.0 Hz (+0,00 %), duty = 75,00 % (+0,00 pp), n = 198 |
| 10 | 1000 Hz | 100 % | sin flancos, nivel ALTO, capturas=0 |
| 11 | 2000 Hz | 0 % | sin flancos, nivel BAJO, capturas=0 |
| 12 | 2000 Hz | 25 % | f = 2000.0 Hz (+0,00 %), duty = 24,99 % (-0,01 pp), n = 398 |
| 13 | 2000 Hz | 50 % | f = 2000.0 Hz (+0,00 %), duty = 50,00 % (+0,00 pp), n = 397 |
| 14 | 2000 Hz | 75 % | f = 2000.0 Hz (+0,00 %), duty = 75,00 % (+0,00 pp), n = 398 |
| 15 | 2000 Hz | 100 % | sin flancos, nivel ALTO, capturas=0 |

- Los 15 casos dieron `OK`. El criterio es error de frecuencia <= 2 % y duty dentro de +-2 puntos (el enunciado pide +-2 puntos para 25, 50 y 75 %).
- Los 9 casos con flancos (25, 50 y 75 % en las tres frecuencias) midieron la frecuencia con error de +0,00 % (la medicion tiene una resolucion de una cuenta de 84 MHz, 11,9 ns).
- El error de duty maximo fue de -0,01 puntos, en los casos de 25 %. Equivale a una cuenta de 11,9 ns en el tiempo en alto, es decir, el limite de resolucion de la captura.
- **0 % y 100 %:** en las tres frecuencias, `sin flancos` y `capturas=0` durante toda la ventana de 200 ms (de 100 a 400 periodos): el nivel es constante, bajo para 0 % y alto para 100 %, y **no hay pulsos residuales**. Esto es lo que se espera de `CCR = 0` y `CCR = ARR + 1` en el modo PWM1 (RM0390, TIM2-TIM5, "PWM mode").
- `n` es el numero de periodos promediados en la ventana de 200 ms (se descartan 2 capturas al empezar): 98 a 500 Hz, 198 a 1 kHz y 397-398 a 2 kHz. Los 397 de la linea 13 son un periodo menos que el resto, sin efecto sobre el resultado.

### Detalle de H3-04 (sin puente)

Con PA0 sin conectar, la entrada (con pull-down) lee siempre 0. Resultado: 3/15 OK. Pasan solo las combinaciones de 0 % (lineas 1, 6 y 11), porque con 0 % lo esperado es precisamente nivel bajo y sin flancos. Las otras 12 (25, 50, 75 y 100 %) dan `sin flancos, nivel BAJO, capturas=0 FALLA`. Esto demuestra que el medidor no da un resultado correcto "por defecto" y que un puente suelto se detecta. Por eso las pruebas de 100 % y de 25/50/75 % son necesarias: la de 0 % sola no basta para comprobar el cableado.

## Pruebas de logica en PC (`pruebas/host/`)

`bash pruebas/host/run_tests.sh` (requiere `gcc` y `bash`) compila con `-Werror`, ASan y UBSan y ejecuta las pruebas de ringbuf, consola, tick, boton, PWM, medidor, verificacion y aplicacion, con un modelo del hardware (`hal_sim.c`). Para el PWM:

- `test_pwm.c`: ARR y CCR de las tres frecuencias, los 101 valores de duty exactos (`CCR * 100 == duty * (ARR + 1)`), 0 % y 100 %, 99 % y 1 % con flancos, entradas invalidas, reloj y preescalador leidos del hardware, precarga y que todas las escrituras de ARR y CCR con el contador en marcha se hacen con `UDIS` puesto.
- `test_meter.c`: registros de TIM2 y de PA0, mediciones exactas, 0 y 100 %, limite de tolerancia (2 %), puente suelto, capturas no validas, sobrecaptura y ventana que cruza UINT32_MAX.
- `test_check.c`: medicion unica, autoprueba de 15 lineas, error de frecuencia, puente suelto, cola TX llena (incluido en el resumen final), restauracion del PWM y tiempo que cruza UINT32_MAX.
- `test_app.c`: teclas `f`, `d`, `p` y `a`; las teclas se rechazan durante la autoprueba; B1 sigue operativo durante la autoprueba.

Para comprobar que las pruebas detectan fallos reales se inyectaron 42 errores a proposito en `pwm.c`, `pwm_meter.c`, `pwm_check.c` y `app.c`; las pruebas detectaron los 42. Estas pruebas verifican la **logica**; no sustituyen a las de la placa.

## Limitaciones de este hito

- **Medicion interna, no instrumento independiente.** TIM2 (medidor) y TIM3 (PWM) cuelgan del mismo reloj (HSI -> PLL a 84 MHz). Si el oscilador tuviera un error, TIM2 lo heredaria y aparecerian errores de frecuencia de 0 %. La medicion valida el calculo de ARR/CCR, el preescalador, los niveles 0 y 100 % y la ausencia de pulsos, pero no la exactitud absoluta del reloj.
- **Exactitud del HSI.** Segun el datasheet del STM32F446, el HSI tiene una exactitud de +-1 % a 25 C, -4/+4 % de -10 a 85 C y -8/+4,5 % de -40 a 105 C. Es un dato del fabricante, no una medicion hecha aqui; a temperatura ambiente de laboratorio queda dentro del 2 % pedido.
- **Sin captura de osciloscopio.** El enunciado pide capturas y mediciones; se aportan las mediciones internas descritas arriba y la aceptacion del metodo por el docente.
- **Pruebas de aceptacion por comando.** DUTY, FREQ y STATUS por la consola no existen todavia (Hito 4): aqui se verifico el nivel PWM con teclas temporales. Se repetiran por comando en el Hito 5.
- **Mirada al LED externo.** No se usa como verificacion de frecuencia ni de duty (el enunciado dice que no los verifica).
- Las teclas `f`, `d`, `p` y `a` son temporales; se sustituyen por el interprete de ordenes en el Hito 4.
