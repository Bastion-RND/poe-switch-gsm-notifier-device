/*
 * sim800_parser.h
 *
 *  Created on: Sep 25, 2021
 *      Author: sa100
 */

#ifndef SIM800_PARSER_H_
#define SIM800_PARSER_H_

#include "sim800_types.h"

#define	CHAR_TO_INT(c)		(c >= '0' && c <= '9') ? (c - '0') : (-1)
#define STRINGS_MATCH		0

/* prototypes */
void sim800_rx_ring_parser(sim800_t*);

//void sim800_readline(sim800_t*, void*);
void sim800_readline(sim800_t* p, char* str, size_t size, uint32_t timeout);
void sim800_str_parser(sim800_t*, void* param);
void sim800_num_parser(sim800_t*, void* param);
void sim800_skip_parse(sim800_t*, int size);
void sim800_ip_addr_parse(sim800_t* p, void* param);

int	 sim800_parse_int(const char** pptr);
void sim800_parse_str(const char** pptr, char* str);

#endif /* SIM800_PARSER_H_ */
