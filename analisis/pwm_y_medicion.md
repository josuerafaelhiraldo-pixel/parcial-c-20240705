# Analisis: PWM por hardware (TIM3) y medicion interna (TIM2)

Aplica a `Src/pwm.c`, `Src/pwm_meter.c`, `Src/pwm_check.c`, `Inc/app_config.h` y `Src/app.c`.

## 1. PWM con TIM3 canal 1 (PA6)

### Reloj y formulas

| Dato | Valor |
|---|---|
| Reloj de los temporizadores de APB1 | 84 MHz (HCLK 84 MHz, APB1 /2 -> PCLK1 42 MHz, y x2 porque el divisor es distinto de 1) |
| Preescalador de TIM3 (PSC) | 83 -> 84 MHz / 84 = 1 MHz (1 us por cuenta) |
| Cuentas por periodo | `(reloj / (PSC + 1)) / frecuencia` |
| ARR | cuentas - 1 |
| CCR1 | `duty * cuentas / 100` |

El reloj **no se supone**: `Pwm_TimerClockHz()` lo calcula en tiempo de ejecucion con `HAL_RCC_GetHCLKFreq()` y `HAL_RCC_GetPCLK1Freq()` (si el divisor de APB1 no es 1, el temporizador recibe 2 x PCLK1). El preescalador se lee del registro PSC. Si alguien cambia el arbol de relojes en CubeMX, el periodo se recalcula solo. La prueba en PC lo comprueba con otros arboles de relojes (72 MHz, APB1 sin dividir, APB1 /4) y con otro PSC.

| Frecuencia | Cuentas | ARR | CCR a 25 % | CCR a 50 % | CCR a 75 % | Paso de duty |
|---|---|---|---|---|---|---|
| 500 Hz | 2000 | 1999 | 500 | 1000 | 1500 | 0,05 % por cuenta |
| 1000 Hz | 1000 | 999 | 250 | 500 | 750 | 0,1 % por cuenta |
| 2000 Hz | 500 | 499 | 125 | 250 | 375 | 0,2 % por cuenta |

Como las cuentas por periodo (2000, 1000, 500) son multiplos de 100, cada duty entero de 0 a 100 % da un CCR entero **exacto**, sin redondeo. La prueba en PC lo verifica para los 101 valores en las tres frecuencias: `CCR * 100 == duty * (ARR + 1)`.

### 0 % y 100 %: niveles constantes, sin pulsos

El modo PWM1 del TIM3 es `OCxREF = 1 mientras CNT < CCRx`. Segun el RM0390 (cap. 17, TIM2-TIM5, §17.3.9 "PWM mode"):

- si el valor de comparacion es **0**, `OCxREF` se mantiene en 0: salida **baja** constante;
- si el valor de comparacion es **mayor que ARR**, `OCxREF` se mantiene en 1: salida **alta** constante.

Por eso 0 % usa `CCR = 0` y 100 % usa `CCR = ARR + 1` (no `ARR`: con `CCR = ARR` habria un pulso bajo de una cuenta por periodo). El hardware no genera ningun flanco en esos dos casos, asi que no hace falta tratamiento especial por software. 99 % y 1 % si tienen flancos (la prueba en PC lo comprueba).

### Cambio de frecuencia o duty sin glitches

- **ARR con precarga (ARPE).** TIM3 es de 16 bits. Sin precarga, si se escribe un ARR menor que la cuenta actual, el contador no se detiene en el nuevo ARR: sigue hasta 65535 y vuelve a 0, es decir, unos 65 ms de salida incorrecta a 1 MHz. `Pwm_Init()` activa `TIM_CR1_ARPE` y la precarga de CCR1, sin depender de como lo deje CubeMX.
- **ARR y CCR entran juntos.** Cada cambio se escribe con `UDIS = 1` (actualizaciones detenidas) y luego `UDIS = 0` (RM0390 §17.3.2: con UDIS puesto "no update event occurs until the UDIS bit has been written to 0"). Los dos valores nuevos se cargan en el siguiente desbordamiento. Sin UDIS, un desbordamiento entre las dos escrituras produciria un periodo con el ARR nuevo y el CCR viejo.
- El cambio se aplica al final del periodo en curso: como maximo 2 ms (500 Hz). No es un evento de actualizacion forzado (`UG`) porque ese reiniciaria el contador a mitad de periodo.
- En `Pwm_Init()` el contador aun no corre: se escriben ARR y CCR1 y se genera un `UG` para cargarlos en los registros activos antes de arrancar con `HAL_TIM_PWM_Start()`.

### El tick de 1 ms no cambia

El tick usa TIM6 (PSC 83, ARR 999), independiente de TIM3. Al cambiar la frecuencia del PWM solo se escribe en TIM3. La autoprueba lo comprueba en la placa: compara cuanto avanzaron TIM6 y SysTick durante los 15 cambios de configuracion (diferencia esperada: 0 ms, o 1 ms por el instante de lectura).

## 2. Medidor interno con TIM2 ("entrada PWM")

### Conexion

Puente (cable Dupont) de **PA6** (CN5 pin 5, D12; salida del PWM) a **PA0** (CN8 pin 1, A0; entrada de captura TIM2_CH1). PA0 se configura con **pull-down**: sin puente la entrada lee 0, y las mediciones de 25/50/75 % y 100 % fallan en vez de dar un falso resultado correcto.

### Configuracion (RM0390 §17.3.6, "PWM input mode")

| Registro | Valor | Significado |
|---|---|---|
| `CCMR1` | `CC1S = 01`, `CC2S = 10` | CCR1 y CCR2 capturan la misma entrada TI1 (PA0); CCR2 por entrada indirecta |
| `CCER` | `CC1E`, `CC2E`, `CC2P = 1` | CCR1 captura en flanco de **subida**; CCR2 en flanco de **bajada** |
| `SMCR` | `TS = 101`, `SMS = 100` | TI1FP1 dispara el modo esclavo "reset": cada flanco de subida pone el contador a 0 |
| `PSC`, `ARR` | 0, 0xFFFFFFFF | TIM2 es de 32 bits y cuenta a 84 MHz (11,9 ns por cuenta) |

Resultado: en cada flanco de subida, `CCR1` = cuentas de **un periodo** y `CCR2` = cuentas del **tiempo en alto**. Los registros se escriben de forma explicita en `Meter_Init()`, asi que el resultado no depende de la configuracion que genere CubeMX para TIM2 (CubeMX se usa para dejar documentado el pin y el temporizador en el `.ioc`).

### Calculo

- Frecuencia = `reloj * N / suma_de_periodos` (en milihertz, con enteros de 64 bits).
- Duty = `suma_de_tiempos_en_alto * 10000 / suma_de_periodos` (en centesimas de %).
- Se promedian todos los periodos de una ventana de **200 ms** (unos 100, 200 y 400 periodos a 500, 1000 y 2000 Hz).
- Se descartan las 2 primeras capturas de cada ventana (pueden ser de la configuracion anterior).
- Resolucion: una cuenta = 11,9 ns. En el peor caso (2000 Hz, 42 000 cuentas por periodo) el error de cuantizacion de un periodo es 0,0024 %, muy por debajo de la tolerancia (2 %).
- Una captura solo cuenta si `periodo != 0` y `alto <= periodo`.

### Sin interrupciones y sin esperas

La medicion no bloquea: `Meter_Start()` la arma y `Meter_Service()` se llama en cada vuelta de `main`, lee la bandera `CC1IF` por sondeo y toma los dos registros. La ventana termina con `(uint32_t)(ahora - inicio) >= 200`, por lo que resiste el desbordamiento del tiempo (prueba en PC cruzando UINT32_MAX). Si `main` tarda mas de un periodo entre dos lecturas, el hardware levanta `CC1OF` (sobrecaptura); se cuenta en `missed`, pero el valor leido sigue siendo un periodo completo y correcto.

### 0 % y 100 %

No hay flancos: `samples = 0`. El medidor devuelve el nivel de PA0. El veredicto exige, ademas, **cero capturas en toda la ventana** (200 ms, es decir 100-400 periodos): asi se comprueba la condicion "sin pulsos residuales" del enunciado. Antes de medir se esperan 20 ms desde el cambio de configuracion (el periodo viejo dura como maximo 2 ms) y se borran las banderas, para que una captura del PWM anterior no cuente como pulso residual.

### Criterios de aceptacion de cada medicion

| Caso | Pasa si |
|---|---|
| 25 / 50 / 75 % | `|error de frecuencia| <= 2,00 %` **y** `|error de duty| <= 2,00 puntos` (el enunciado pide +-2 puntos) y al menos 10 periodos medidos |
| 0 % | sin capturas en la ventana y PA0 en bajo |
| 100 % | sin capturas en la ventana y PA0 en alto |

## 3. Autoprueba (tecla `a`)

Recorre 500 / 1000 / 2000 Hz x duty 0 / 25 / 50 / 75 / 100 % = 15 combinaciones. Para cada una: aplica la configuracion, espera 20 ms, mide 200 ms, imprime una linea con el veredicto y pasa a la siguiente. Dura unos 3,3 s (15 x 220 ms), es una maquina de estados (sin esperas) y al final restaura la frecuencia y el duty que habia. Si la cola de transmision esta llena, la linea se reintenta en la vuelta siguiente (nada se pierde). La tecla `p` mide solo la configuracion actual.

## 4. Limitaciones (importante)

1. **No es un instrumento independiente.** TIM2 (medidor) y TIM3 (PWM) cuelgan del mismo reloj (HSI -> PLL). La medicion comprueba la **logica y los registros** (preescalador, ARR, CCR, niveles 0/100 %, ausencia de pulsos, conexion del pin), pero si el oscilador estuviera desviado, TIM2 se desviaria igual y el error seguiria apareciendo como 0 %.
2. **Exactitud absoluta del reloj.** El oscilador es el HSI de 16 MHz. Segun el datasheet del STM32F446 (tabla "HSI oscillator characteristics"), su exactitud es +-1 % a 25 C (calibrado de fabrica), -4 / +4 % entre -10 y 85 C, y -8 / +4,5 % entre -40 y 105 C. A temperatura ambiente de laboratorio, el error esperado de frecuencia queda dentro de +-1 %, es decir, dentro del 2 % pedido. Esto es un dato del fabricante, no una medicion.
3. Un osciloscopio o analizador logico seguiria siendo la forma de verificar frecuencia y duty de forma independiente; aqui se documenta el metodo alternativo usado por no disponer de uno.
4. La mirada al LED externo no verifica frecuencia ni duty (lo dice el enunciado); solo sirve para ver que el brillo cambia.

## 5. Pruebas en PC (`pruebas/host/`)

`run_tests.sh` compila con gcc (ASan + UBSan, `-Werror`) y ejecuta, entre otras, estas pruebas con un modelo del hardware (`hal_sim.c`):

- `test_pwm.c`: ARR/CCR de las 3 frecuencias, los 101 duties exactos, 0/100 %, 99/1 % con flancos, entradas invalidas, reloj y PSC reales, ARPE/UG/precarga, y que **todas** las escrituras de ARR y CCR con el contador en marcha se hacen con UDIS puesto.
- `test_meter.c`: registros de TIM2 y de PA0 (pull-down), mediciones exactas, 0/100 %, tolerancia (limite 2 %), puente suelto, capturas no validas, sobrecaptura, ventana cruzando UINT32_MAX.
- `test_check.c`: medicion unica, autoprueba de 15 lineas, deteccion de error de frecuencia, puente suelto, cola TX llena, restauracion del PWM, tiempo cruzando UINT32_MAX.
- `test_app.c`: teclas `f`, `d`, `p`, `a`, teclas rechazadas durante la autoprueba, B1 operativo durante la autoprueba.

Para comprobar que las pruebas detectan fallos reales se inyectaron 42 errores a proposito (mutantes) en `pwm.c`, `pwm_meter.c`, `pwm_check.c` y `app.c` (ARR sin restar 1, 100 % con `CCR = ARR`, sin UDIS, reloj supuesto en 84 MHz, tolerancia de 3 %, 0 % que acepta pulsos, comparacion directa de tiempos, etc.): las pruebas detectaron los 42. Estas pruebas verifican la **logica**; no sustituyen las de la placa.

## 6. Codigo del SDK y codigo propio

| Parte | Origen |
|---|---|
| `MX_TIM3_Init`, `MX_TIM2_Init`, `HAL_TIM_PWM_MspInit`, GPIO de PA6 | Generado por CubeMX (SDK) |
| Llamadas `HAL_TIM_PWM_Start`, `HAL_TIM_GenerateEvent`, `HAL_GPIO_Init`, `HAL_RCC_Get*Freq` | HAL de ST (SDK) |
| `pwm.c`: calculo de ARR/CCR con el reloj real, UDIS, precarga, 0/100 % | Propio |
| `pwm_meter.c`: registros de TIM2, sondeo de capturas, promedios, veredicto | Propio |
| `pwm_check.c`: maquina de estados de la autoprueba y sus mensajes | Propio |
| `pruebas/host/*` | Propio |
