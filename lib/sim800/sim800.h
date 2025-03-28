/*
 * sim800.h
 *
 *  Created on: Sep 21, 2021
 *      Author: sa100
 */

#ifndef SIM800_H_
#define SIM800_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


#include "sim800_conf.h"
#include "sim800_types.h"
#include "sim800_const.h"
#include "sim800_parser.h"

#include "sim800_gsm.h"
#include "sim800_gprs.h"


#define COMMAND_TIMEOUT		1000/* ms */


/* prototypes */
sim800_t*		sim800_init(int index);
void 			sim800_restart(sim800_t*);
void 			sim800_reset(sim800_t*);
void			sim800_uart_handler(sim800_t*);
void			sim800_run(sim800_t*);
bool			sim800_parser_add(sim800_t*, const char*, sim800_parser_handler_t, void* param);
bool			sim800_parser_remove(sim800_t*, const char*);
sim800_Result_t	sim800_cmd(sim800_t*, const char*, uint32_t to, sim800_callback_t, void* param, sim800_Flow_t);

void    sim800_unlock(sim800_t* p);
bool    sim800_is_locked(sim800_t* p);


/* callback`s */
void	on_pin_checked_callback(sim800_t*, const char* status);
void	on_rssi_updated_callback(sim800_t*, int dBm);







#endif /* SIM800_H_ */
