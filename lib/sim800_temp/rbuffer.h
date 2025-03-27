/*
 * rbuffer.h
 *
 *  Created on: Sep 22, 2021
 *      Author: sa100
 */

#ifndef SIM800_RBUFFER_H_
#define SIM800_RBUFFER_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


/* typedefs */

typedef struct ringbuffer_
{
    size_t   			size;
    uint8_t* 			pData;
    volatile uint8_t* 	pHead;
    volatile uint8_t* 	pTail;
    volatile bool 		overrun;
    volatile bool 		empty;

} ringbuffer_t;


/* prototypes */

void	rb_init(ringbuffer_t*, uint8_t*, size_t);

int		rb_available(ringbuffer_t*);
int		rb_available_for_write(ringbuffer_t*);

uint8_t rb_at(ringbuffer_t*, int index);
uint8_t	rb_peek(ringbuffer_t*);

uint8_t	rb_read_byte(ringbuffer_t*);
size_t	rb_read(ringbuffer_t*, uint8_t*, size_t);
size_t	rb_skip(ringbuffer_t*, size_t);

void	rb_write_byte(ringbuffer_t*, uint8_t);
size_t	rb_write(ringbuffer_t*, uint8_t*, size_t);


#endif /* SIM800_RBUFFER_H_ */
