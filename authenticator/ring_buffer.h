#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>

struct ring_buffer {
    uint8_t *buffer;
    uint8_t head;
    uint8_t tail;
    uint8_t maxlen;
};

/*
 * Initializes an already allocated ring_buffer struct
 * <rb>: a pointer to a ring_buffer struct
 * <buffer>: an already allocated buffer that the ring_buffer will use
 * <buffer_size>: the size of the <buffer>
 *
 * Remark: this can not fail
*/
void ring_buffer__init(struct ring_buffer* rb, uint8_t* buffer, uint8_t buffer_size);

/*
 * Pushes a byte in a ring buffer
 * <rb>: a pointer to a ring_buffer struct
 * <data>: the byte to push
 *
 * Remark: this can not fail
*/
void ring_buffer__push(struct ring_buffer* rb, uint8_t data);

/*
 * Pops a byte from a ring buffer
 * <rb>: a pointer to a ring_buffer struct
 * <data>: the address where the byte will be written
 * @return:
 *  0 if pop succeed
 *  1 if pop failed because ring buffer is empty
*/
uint8_t ring_buffer__pop(struct ring_buffer* rb, uint8_t* data);

#endif
