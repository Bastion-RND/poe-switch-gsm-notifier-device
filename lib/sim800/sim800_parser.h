#ifndef SIM800_PARSER_H_
#define SIM800_PARSER_H_

#include "sim800_types.h"

#define	CHAR_TO_INT(c)		(c >= '0' && c <= '9') ? (c - '0') : (-1)
#define STRINGS_MATCH		0

void sim800_rx_ring_parser();

void sim800_readline(Sim800Handle_t*, char* str, size_t size, uint32_t timeout);
void sim800_str_parser(Sim800Handle_t*, void* param);
void sim800_num_parser(Sim800Handle_t*, void* param);
void sim800_skip_parse(Sim800Handle_t*, int size);
void sim800_ip_addr_parse(Sim800Handle_t*, void* param);

int	 sim800_parse_int(const char** pptr);
void sim800_parse_str(const char** pptr, char* str);

#endif /* SIM800_PARSER_H_ */
