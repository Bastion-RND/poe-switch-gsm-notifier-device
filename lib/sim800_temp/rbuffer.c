/*
 * rbuffer.c
 *
 *  Created on: Sep 22, 2021
 *      Author: sa100
 */

#include <string.h>

#include "rbuffer.h"
//#include "Debug.h"

static void
move_head(ringbuffer_t* rb, size_t offset)
{
    volatile int head;

    head = rb->pHead - rb->pData;
    head = (head + offset) % rb->size;
//    head %= rb->size;

    if (offset > rb_available_for_write(rb)) {
        rb->pTail = rb->pData + head;
        rb->overrun = true;
    }
    rb->pHead = rb->pData + head;
    rb->empty = false;
}

static void
move_tail(ringbuffer_t* rb, size_t offset)
{
	volatile int tail;

    tail = rb->pTail - rb->pData;
    tail = (tail + offset) % rb->size;
//    tail %= rb->size;

    if (offset > rb_available(rb)) {
        rb->pHead = rb->pData + tail;
    }
    rb->pTail = rb->pData + tail;
    rb->empty = (rb->pTail == rb->pHead);
    rb->overrun = false;

}

void
rb_init(ringbuffer_t* rb, uint8_t* data, size_t size)
{
    if (rb && data && size)
    {
        memset(data, 0x00, size);
        rb->overrun = false;
        rb->empty = true;
        rb->size = size;

        rb->pData = data;
        rb->pHead = data;
        rb->pTail = data;
    }
}

int
rb_available(ringbuffer_t* rb)
{
    volatile int head;
    volatile int tail;
    volatile int result;

    head = rb->pHead - rb->pData;
    tail = rb->pTail - rb->pData;

    if (rb->empty) {
        result = 0;
    }
    else if (head > tail) {
        result = head - tail;
    }
    else {
        result = rb->size - tail + head;
    }
    return result;
}

int
rb_available_for_write(ringbuffer_t* rb)
{
    return (rb->size - rb_available(rb));
}

uint8_t
rb_at(ringbuffer_t* rb, int index)
{
    int offset;

    if (rb_available(rb) > index)
    {
        offset = rb->pTail - rb->pData + index;
        offset %= rb->size;

        return *(rb->pData + offset);
    }
    return 0;
}

uint8_t
rb_peek(ringbuffer_t* rb)
{
    return (rb_available(rb)) ? *(rb->pTail) : 0;
}


uint8_t
rb_read_byte(ringbuffer_t* rb)
{
	volatile uint8_t byte;

    byte = (rb->empty) ? 0x00 : *(rb->pTail);
    move_tail(rb, 1);
    return byte;
}

size_t
rb_read(ringbuffer_t* rb, uint8_t* pRxData, size_t size)
{
    size_t bytes = 0;

    while (size && rb_available(rb))
    {
        *(pRxData + bytes) = rb_read_byte(rb);
        bytes++;
        size--;
    }
    return bytes;
}

size_t
rb_skip(ringbuffer_t* rb, size_t size)
{
    size_t bytes = 0;

    while (size && rb_available(rb))
    {
        rb_read_byte(rb);
        bytes++;
        size--;
    }
    return bytes;
}

void
rb_write_byte(ringbuffer_t* rb, uint8_t byte)
{
    volatile uint8_t* ptr = rb->pHead; //FIXME!

    *(rb->pHead) = byte;
    move_head(rb, 1);

    if (rb->pHead == ptr) {
//    	printf("ERROR!!!\n");
    }
}

size_t
rb_write(ringbuffer_t* rb, uint8_t* pTxData, size_t size)
{
    size_t bytes = 0;

    while (size)
    {
        rb_write_byte(rb, *(pTxData + bytes));
        bytes++;
        size--;
    }
    return bytes;
}




