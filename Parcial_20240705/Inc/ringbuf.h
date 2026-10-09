/* ringbuf.h - Buffer circular de bytes.
 *
 * Seguro entre UN productor y UN consumidor que corren en contextos
 * distintos (por ejemplo, ISR produce y main consume), porque cada indice
 * lo escribe un solo lado:
 *   - head: lo escribe solo el productor.
 *   - tail: lo escribe solo el consumidor.
 * El tamano debe ser potencia de 2. Capacidad util = tamano - 1. */
#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t          *buf;
    uint16_t          mask;   /* tamano - 1 */
    volatile uint16_t head;   /* proxima posicion a escribir */
    volatile uint16_t tail;   /* proxima posicion a leer */
} RingBuf;

/* Devuelve false si size no es potencia de 2 (o es 0 / mayor a 32768). */
bool     RingBuf_Init(RingBuf *rb, uint8_t *storage, uint16_t size);

bool     RingBuf_Put(RingBuf *rb, uint8_t b);      /* productor */
bool     RingBuf_Get(RingBuf *rb, uint8_t *b);     /* consumidor */

uint16_t RingBuf_Count(const RingBuf *rb);         /* bytes guardados */
uint16_t RingBuf_Free(const RingBuf *rb);          /* bytes libres */

/* Para transmitir en bloque: tramo contiguo desde tail (sin dar la vuelta). */
uint16_t       RingBuf_Contiguous(const RingBuf *rb);
const uint8_t *RingBuf_TailPtr(const RingBuf *rb);
void           RingBuf_Skip(RingBuf *rb, uint16_t n);   /* consumidor */

#endif /* RINGBUF_H */
