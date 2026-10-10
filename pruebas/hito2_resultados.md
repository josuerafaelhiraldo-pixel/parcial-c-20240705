# Hito 2 - Interrupciones, tick de 1 ms y antirrebote: resultados

- Fecha: 2026-10-10
- Placa: NUCLEO-F446RE (B1 en PC13, USART2 por el ST-LINK)
- Herramientas: STM32CubeIDE 2.2.0, STM32CubeMX 6.18.1
- Terminal: Tera Term, COM8, 115200 baudios, 8N1, sin control de flujo
- Codigo probado: `tick.c`, `button.c`, `app.c` del Hito 2 (commits "Agrego tick de 1 ms con interrupcion de TIM6", "Agrego boton B1 con EXTI en ambos flancos y antirrebote de 30 ms" y "Hito 2: integro tick y boton en la app, con pruebas en PC y analisis")
- Diseno y calculos: `analisis/tick_y_antirrebote.md`

## Compilacion y manejadores de interrupcion

- Primer build del Hito 2: `0 errors, 0 warnings` (text 23 124 B, data 92 B, bss 4 028 B), captura de las 23:28.
- Despues de esa captura solo cambio `app.c` (los mensajes `[B1]` muestran `t=` y la tecla `w` pone el contador a 2048 ms de UINT32_MAX). Se recompilo y se cargo en la placa; las pruebas de este documento usan ese codigo final.
- `Src/stm32f4xx_it.c` (generado por CubeMX): `TIM6_DAC_IRQHandler` llama a `HAL_TIM_IRQHandler(&htim6)` y `EXTI15_10_IRQHandler` llama a `HAL_GPIO_EXTI_IRQHandler(BTN_USER_Pin)` (PC13). Las dos funciones propias que se ejecutan en interrupcion son `HAL_TIM_PeriodElapsedCallback` (`s_ms++`) y `HAL_GPIO_EXTI_Callback` (`s_isrEdges++`).

## Resultados en la placa

| ID | Prueba | Resultado esperado | Resultado observado | Estado | Evidencia |
|---|---|---|---|---|---|
| H2-01 | Estado inicial tras RESET | Contadores en 0, B1 en reposo = 1 | `flancos_isr=0 cortas=0 largas=0`, `B1 (PC13) en reposo: nivel 1` | OK | `evidencias/hito2/03a_estado_inicial.png` |
| H2-02 | TIM6 contra SysTick (tecla `m`), 20 comparaciones | TIM6 cuenta 1 ms por interrupcion | 17 comparaciones con diferencia 0 y 3 con 1 ms o -1 ms (cuantizacion del muestreo). En unos 9,5 s la diferencia acumulada fue de 1 ms (TIM6 9507 ms, SysTick 9508 ms) | OK | `evidencias/hito2/02_tick_tim6_vs_systick.png` |
| H2-03 | Tiempo cerca de UINT32_MAX con `m` | La temporizacion continua al dar la vuelta | TIM6 paso de 4294963200 a 4321 ms; el intervalo medido fue `+8417` en TIM6 y `+8417` en SysTick, diferencia 0 | OK | `evidencias/hito2/02_tick_tim6_vs_systick.png` |
| H2-04 | 20 pulsaciones cortas separadas | 20 eventos, sin duplicados | `cortas=20 largas=0 flancos_isr=40`; mensajes `corta #1` a `corta #20` con duraciones de 158 a 220 ms y unos 500 ms entre ellas | OK | `evidencias/hito2/03_20_cortas_y_estadisticas.png` |
| H2-05 | Mantener B1 unos 2 s y soltar | Una larga; al soltar no sale corta | Un `larga #1`; `cortas` sigue en 20; `flancos_isr` pasa de 40 a 42; `ultima=1950 ms` | OK | `evidencias/hito2/04_larga_y_estadisticas.png` |
| H2-06 | Mantener B1 10 s y soltar | Una sola larga, sin repeticiones | `larga #1`; `cortas=0 largas=1 flancos_isr=2 ultima=10702 ms` | OK | `evidencias/hito2/04b_larga_10s.png` |
| H2-07 | Arrancar con B1 pulsado 2 s y soltar | Esa pulsacion no genera eventos | `flancos_isr=1 cortas=0 largas=0 ultima=2336 ms` | OK | `evidencias/hito2/06_arranque_con_b1_pulsado.png` |
| H2-08 | Larga que cruza el desbordamiento (tecla `w`, luego B1 3 s) | La larga sale 1500 ms despues de validar, aunque el contador de la vuelta | Tres intentos tras `w` (`Tick_Ms = 4294965248`, a 2048 ms de la vuelta): `t=13594 ms` y `t=1764 ms` (pulse tarde: ya habia dado la vuelta) y `t=628 ms` en el tercero | OK (tercer intento) | `evidencias/hito2/05_desbordamiento_uint32.png` |

### Como se interpreta H2-08

Una larga se emite exactamente 1500 ms despues de validar la pulsacion. En el tercer intento el mensaje salio con `t=628 ms`, un valor ya posterior a la vuelta del contador (que ocurre al pasar de 4294967295 a 0). La pulsacion se valido entonces en `628 - 1500 = -872 ms`, es decir, 872 ms **antes** de la vuelta, y el temporizador de 1500 ms la cruzo con la resta `(uint32_t)(ahora - antes)`. En los dos primeros intentos `t` fue mayor de 1500 ms, lo que indica que la pulsacion empezo despues de la vuelta; por eso no cuentan como prueba del cruce y se dejan visibles en la captura.

## Pruebas de logica en PC (`pruebas/host/`)

`test_button.c` y `test_tick.c` ejercitan el codigo propio con un reloj simulado de 1 ms. Cubren:

- rebotes al pulsar y al soltar (10 flancos para una sola pulsacion), pulsos de ruido de 10 y 25 ms y un flanco de la ISR sin cambio de nivel;
- 20 cortas con rebotes, una larga de 3 s y una de 10 s (sin repeticion ni corta al soltar);
- un barrido de 1400 a 1600 ms de pulsacion (siempre un solo evento: corta si lo validado es menor de 1500 ms y larga si es de 1500 ms o mas) y otro de 0 a 120 ms (ningun evento menor de 30 ms);
- pulsaciones cortas y largas que cruzan UINT32_MAX y arranque con B1 pulsado.

Se ejecutan con `bash pruebas/host/run_tests.sh` (requiere `gcc` y `bash`). Comprueban la logica, no el hardware.

## Limitaciones de este hito

- **Rebotes reales:** el boton de la placa casi no rebotaba en estas pruebas (se vieron 2 flancos por pulsacion: 40 flancos para 20 pulsaciones). El filtro de 30 ms se verifica con rebotes simulados en las pruebas de PC, no con rebotes reales en la placa.
- **Tiempos medidos por el propio firmware:** las duraciones (`duracion`, `ultima`) y `t` salen del contador de TIM6. TIM6 y SysTick comparten el reloj HCLK, asi que la comparacion `m` verifica la cuenta de TIM6, no la exactitud absoluta del reloj interno.
- **Tiempo de respuesta del boton (50 ms):** no se midio con instrumento. El evento se imprime en la misma vuelta de `main` en que se valida; un mensaje de unos 45 caracteres tarda unos 4 ms en salir por la UART.
- Las teclas de prueba y las acciones de B1 (corta = alternar LD2, larga = apagar LD2) son temporales; se sustituyen por la maquina de estados y el interprete de ordenes en el Hito 4.
