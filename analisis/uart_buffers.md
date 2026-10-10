# Analisis de tiempos y buffers de la consola UART

Aplica a `Src/console.c`, `Src/ringbuf.c` y `Inc/app_config.h`.
Placa NUCLEO-F446RE, USART2 a 115200 baudios, 8N1 (PA2 TX, PA3 RX).

## 1. Tiempo por byte

Una trama 8N1 tiene 1 bit de inicio + 8 de datos + 1 de parada = 10 bits.

- Tiempo por byte = 10 / 115200 = **86,8 us**
- Velocidad maxima = 115200 / 10 = **11 520 bytes/s**
- Ciclos de CPU por byte a 84 MHz = 86,8 us x 84 MHz = **unos 7 290 ciclos**

## 2. Buffer de recepcion (RX)

| Dato | Valor |
|---|---|
| Tamano (`APP_RX_BUF_SIZE`) | 256 bytes (potencia de 2) |
| Capacidad util | 255 bytes (size - 1) |
| Tiempo en llenarse a velocidad maxima | 255 x 86,8 us = **22,1 ms** |
| Linea maxima del enunciado | 63 caracteres + terminador |
| Tiempo de una linea de 64 bytes | 64 x 86,8 us = 5,6 ms |

- Se usa tamano potencia de 2 para calcular el indice con una mascara (`& 255`) en lugar de una division.
- Se sacrifica una posicion para distinguir "lleno" de "vacio" sin una variable extra: con `head == tail` el buffer esta vacio, y con `head + 1 == tail` (modulo 256) esta lleno.
- **Consecuencia de diseno:** si `main` deja de vaciar el buffer mas de 22 ms mientras llega trafico continuo, se pierden bytes. Por eso `App_Loop` procesa como maximo 32 bytes por vuelta (`APP_BYTES_PER_LOOP`) y no puede haber esperas bloqueantes largas en el programa.
- Si el buffer se llena, el byte nuevo se descarta y se incrementa `rxRingOverflow`. La recepcion sigue activa y se recupera en cuanto `main` vacia el buffer.

## 3. Buffer de transmision (TX)

| Dato | Valor |
|---|---|
| Tamano (`APP_TX_BUF_SIZE`) | 512 bytes |
| Capacidad util | 511 bytes |
| Tiempo en vaciarse a velocidad maxima | 511 x 86,8 us = **44,4 ms** |
| Un mensaje de 100 caracteres | 100 x 86,8 us = 8,7 ms |

- Un mensaje se encola completo o no se encola (todo o nada). Si no cabe, se descarta entero y se incrementa `mensajes_descartados` (`txDropped`). Asi nunca sale una linea cortada por la mitad.
- La impresion no espera: `Console_Printf` escribe en el buffer y la ISR de fin de transmision va enviando los trozos contiguos.

## 4. Margen contra desbordamiento del hardware (ORE)

El USART2 tiene un registro de datos (DR) y un registro de desplazamiento. Si llega un byte nuevo y el anterior no se ha leido de DR, el hardware marca *overrun* (ORE).

- Margen: la ISR de RX tiene **menos de 86,8 us (unos 7 290 ciclos)** para leer el byte y rearmar la recepcion.
- La ISR hace lo minimo: la funcion `HAL_UART_RxCpltCallback` guarda el byte en el buffer circular y rearma `HAL_UART_Receive_IT` para el siguiente.
- Si ocurre ORE, FE, NE o PE, `HAL_UART_ErrorCallback` incrementa el contador correspondiente y rearma la recepcion. Los contadores salen con la tecla `s` (y luego con `STATUS`).

## 5. Regla de un productor y un consumidor

Cada buffer tiene un unico escritor para cada indice, por lo que no necesita desactivar interrupciones para leer o escribir bytes:

| Buffer | Productor (escribe `head`) | Consumidor (escribe `tail`) |
|---|---|---|
| RX | ISR de USART2 | `main` (`Console_ReadByte`) |
| TX | `main` (`Console_Write...`) | ISR de fin de transmision |

- Se usa una barrera de memoria (`__DMB()`) entre escribir el dato y publicar el indice.
- Solo el arranque de una transmision TX usa una seccion critica corta (se guarda y restaura PRIMASK), para evitar que `main` y la ISR arranquen dos envios a la vez.

## 6. Preguntas que debo saber responder en la defensa

- **Por que 256 y no 200?** Potencia de 2: el indice se calcula con una mascara, sin division.
- **Por que solo caben 255 bytes?** Una posicion se reserva para distinguir lleno de vacio.
- **Que pasa si llegan 300 bytes seguidos y `main` no los lee?** Se guardan 255, se descartan los demas y `rxRingOverflow` cuenta los perdidos. Despues se recupera sola.
- **Cuanto tiempo puede tardar `main` en volver a leer?** Menos de 22 ms en el peor caso (trafico continuo a maxima velocidad).
- **Por que no se necesita bloquear interrupciones al leer el buffer RX?** Porque `head` solo lo escribe la ISR y `tail` solo lo escribe `main`.
- **Que hace la ISR?** Guardar el byte, contar y rearmar. Nada de imprimir ni interpretar comandos.
