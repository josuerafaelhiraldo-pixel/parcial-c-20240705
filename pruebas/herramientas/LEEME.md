# Macros de Tera Term para las pruebas de aceptacion (Hito 5)

Las pruebas que necesitan bytes exactos o un ritmo fijo (CR/LF/CRLF, lineas largas, 10 ordenes por segundo,
desbordamiento de RX) se ejecutan con macros de Tera Term (`.ttl`). Cada macro envia las ordenes por
COM8 y guarda todo lo que se ve en la consola (eco, respuestas y telemetria) en
`evidencias/hito5/<nombre>.log`.

## Como ejecutar una macro

1. Crear la carpeta `evidencias/hito5/` si no existe.
2. Conectar Tera Term a COM8 (115200, 8N1, Local echo apagado, Enter = CR).
3. Pulsar RESET de la placa y esperar el banner (cada prueba parte de contadores en cero).
4. Menu **Control > Macro...**, elegir el archivo `.ttl` y abrir.
5. Esperar el mensaje "Prueba terminada".

Si el repositorio esta en otra ruta, cambiar la linea `logfile = '...'` al inicio de cada macro.

## Pruebas

| Macro | Prueba de aceptacion |
|---|---|
| `prueba_00_humo.ttl` | Comprueba que el macro y el registro funcionan |
| `prueba_01_duty_freq.ttl` | DUTY 0 / 50 / 100 y FREQ 500 / 1000 / 2000 con medicion interna (puente PA6-PA0); el tick sigue en 1 ms (telemetria cada 1000 ms durante los cambios) |
| `prueba_02_auto_pausa.ttl` | AUTO durante 3,2 s por ciclo, PAUSA y RESUME |
| `prueba_03_errores_y_lineas.ttl` | DUTY 20abc / -1 / 101 y otros errores; limites de linea de 63, 64 y 100 caracteres |
| `prueba_04_terminadores.ttl` | CR, LF y CRLF: una ejecucion por orden |
| `prueba_05_carga_10_por_segundo.ttl` | STREAM ON con 300 ordenes a unas 10 por segundo (unos 30 s) |
| `prueba_06_overflow_rx.ttl` | Desbordamiento de RX provocado con `DIAG STALL` y recuperacion |
| `prueba_07_desbordamiento_uint32.ttl` | Tiempo cerca de UINT32_MAX (`DIAG WRAP`) con AUTO y telemetria |

La prueba 05 pide pulsar B1 (corta) unas 6 veces mientras corre.

Las pruebas de boton (20 pulsaciones cortas y mantener 3 s) no se pueden automatizar: se hacen a mano con
`STREAM ON`, y la consola muestra una linea `EVT B1` por pulsacion.

## Analisis de los registros

El script `analizar_logs.py` (Python 3, solo biblioteca estandar) lee los `.log` de `evidencias/hito5/`
y comprueba cada prueba contra el resultado esperado; sus resultados se resumen en `pruebas/hito5_resultados.md`.
