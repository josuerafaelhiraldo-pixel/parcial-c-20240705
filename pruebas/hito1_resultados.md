# Hito 1 - GPIO y consola: resultados

- Fecha: 2026-10-09
- Placa: NUCLEO-F446RE (LD2 en PA5, B1 en PC13, USART2 en PA2/PA3 por el ST-LINK)
- Herramientas: STM32CubeIDE 2.2.0, STM32CubeMX 6.18.1
- Terminal: Tera Term, COM8, 115200 baudios, 8N1, sin control de flujo
- Codigo probado: commit "Hito 1: pruebo LD2, lectura de B1 y consola por USART2"

## Compilacion

- `0 errors, 0 warnings`
- Tamano: text 21 232 B, data 92 B, bss 3 980 B (total 25 304 B)

## Resultados

| ID | Prueba | Resultado esperado | Resultado observado | Estado | Evidencia |
|---|---|---|---|---|---|
| H1-01 | Compilar el proyecto | 0 errores, 0 advertencias | 0 errores, 0 advertencias | OK | `evidencias/hito1/01_compilacion_0_errores.png` |
| H1-02 | Cargar en la placa por ST-LINK | Carga verificada | `Download verified successfully` | OK | `evidencias/hito1/02_carga_en_placa.png` |
| H1-03 | Reiniciar y leer el banner | Banner y nivel de B1 en reposo | Banner correcto; `B1 (PC13) en reposo: nivel 1` | OK | `evidencias/hito1/03_consola_tera_term.png` |
| H1-04 | Tecla `h` | Lista de teclas de prueba | Lista mostrada | OK | `03_consola_tera_term.png` |
| H1-05 | Tecla `1` | LD2 encendido (PA5 en alto) | Mensaje `LD2 encendido (PA5 en alto)` y LED verde encendido | OK | `03_consola_tera_term.png`, `04_ld2_encendido.jpg` |
| H1-06 | Tecla `0` | LD2 apagado (PA5 en bajo) | Mensaje `LD2 apagado (PA5 en bajo)` y LED apagado | OK | `03_consola_tera_term.png` |
| H1-07 | Tecla `t` tras `1` y `0` | LD2 cambia de estado | Mensaje `LD2 alternado`; el LED quedo encendido | OK | `03_consola_tera_term.png` |
| H1-08 | Tecla `b` con B1 suelto | Nivel de B1 en reposo | `B1 (PC13) nivel = 1` | OK | `03_consola_tera_term.png` |
| H1-09 | Pulsar y soltar B1 | Cambio de nivel informado | Al pulsar `PC13 = 0 (bajo)`; al soltar `PC13 = 1 (alto)` | OK | `03_consola_tera_term.png` |
| H1-10 | Tecla `s` tras 6 teclas | Contadores coherentes, sin errores | RX bytes=6, overflow_buffer=0; ORE=0 FE=0 NE=0 PE=0; TX bytes=409, mensajes_descartados=0 | OK | `03_consola_tera_term.png` |
| H1-11 | Rafaga de mas de 128 caracteres y tecla `s` | Sin perdidas ni errores UART | RX bytes=157 (151 mas que antes); overflow_buffer=0; ORE=0 FE=0 NE=0 PE=0; TX bytes=680, mensajes_descartados=0 | OK | `03_consola_tera_term.png` |

## Conclusiones

- **Polaridad de LD2:** activa en alto (PA5 en alto = LED encendido).
- **Polaridad de B1:** activa en bajo. En reposo PC13 = 1 (con el pull-up interno activado en CubeMX) y al pulsar PC13 = 0.
- Los contadores de la consola confirman que una rafaga mayor que 128 bytes pasa sin perdidas ni errores del USART a velocidad normal.

## Alcance y limitaciones de este hito

- La lectura de B1 de este hito es por sondeo (polling) y solo sirve para medir la polaridad. En el Hito 2 se reemplaza por interrupcion EXTI con antirrebote de 30 ms.
- Las teclas de prueba (`1`, `0`, `t`, `b`, `s`, `h`) son temporales. En el Hito 4 se reemplazan por el parser de comandos del enunciado.
- El desborde forzado del buffer RX y el caso de linea de 100 caracteres se prueban en el Hito 4 y en las pruebas de aceptacion finales.

## Pruebas de logica en PC (opcionales)

En `pruebas/host/` hay pruebas del buffer circular, de la consola y de `app.c` que se compilan en un PC con funciones simuladas del HAL. Se ejecutan con:

```
bash pruebas/host/run_tests.sh
```

Requieren `gcc` y `bash` (por ejemplo en WSL o Git Bash con MinGW). Comprueban la logica, no el hardware: la evidencia en la placa es la tabla de arriba.
