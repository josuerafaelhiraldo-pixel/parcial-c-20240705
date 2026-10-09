/* ringbuf.c - Buffer circular de bytes (1 productor, 1 consumidor). */
#include "ringbuf.h"

#if defined(__ARM_ARCH)
#include "stm32f4xx.h"
#define RB_BARRIER()  __DMB()   /* barrera de memoria y de compilador */
#else
#define RB_BARRIER()  __asm__ volatile("" ::: "memory")   /* pruebas en PC */
#endif

bool RingBuf_Init(RingBuf *rb, uint8_t *storage, uint16_t size)
{
    if ((size == 0U) || (size > 32768U) || ((size & (size - 1U)) != 0U)) {
        return false;
    }
    rb->buf  = storage;
    rb->mask = (uint16_t)(size - 1U);
    rb->head = 0U;
    rb->tail = 0U;
    return true;
}

bool RingBuf_Put(RingBuf *rb, uint8_t b)
{
    uint16_t head = rb->head;
    uint16_t next = (uint16_t)((head + 1U) & rb->mask);

    if (next == rb->tail) {
        return false;                 /* lleno: no se sobrescribe nada */
    }
    rb->buf[head] = b;
    RB_BARRIER();                     /* el dato debe quedar escrito antes de publicar head */
    rb->head = next;
    return true;
}

bool RingBuf_Get(RingBuf *rb, uint8_t *b)
{
    uint16_t tail = rb->tail;

    if (tail == rb->head) {
        return false;                 /* vacio */
    }
    *b = rb->buf[tail];
    RB_BARRIER();                     /* leer el dato antes de liberar la posicion */
    rb->tail = (uint16_t)((tail + 1U) & rb->mask);
    return true;
}

uint16_t RingBuf_Count(const RingBuf *rb)
{
    return (uint16_t)((rb->head - rb->tail) & rb->mask);
}

uint16_t RingBuf_Free(const RingBuf *rb)
{
    return (uint16_t)(rb->mask - RingBuf_Count(rb));
}

uint16_t RingBuf_Contiguous(const RingBuf *rb)
{
    uint16_t tail    = rb->tail;
    uint16_t count   = (uint16_t)((rb->head - tail) & rb->mask);
    uint16_t toEnd   = (uint16_t)((rb->mask + 1U) - tail);

    return (count < toEnd) ? count : toEnd;
}

const uint8_t *RingBuf_TailPtr(const RingBuf *rb)
{
    return &rb->buf[rb->tail];
}

void RingBuf_Skip(RingBuf *rb, uint16_t n)
{
    rb->tail = (uint16_t)((rb->tail + n) & rb->mask);
}
