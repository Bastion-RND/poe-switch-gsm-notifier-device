/*
 * sim800_gprs.h
 *
 *  Created on: Sep 27, 2021
 *      Author: sa100
 */

#ifndef SIM800_GPRS_H_
#define SIM800_GPRS_H_

#include "sim800_types.h"
#include "sim800_const.h"
#include "sim800_parser.h"


#define SIM800_GPRS_PACKET_SIZE_MAX	1460


/* prototypes */
void	sim800_gprs_init(sim800_t*);
void	sim800_gprs_run(sim800_t*);



/* API */
bool	gprs_enable(sim800_t*);
bool	gprs_disable(sim800_t*);


sim800GprsClient_t* gprs_client_connect(sim800_t*, gprsClientParameters_t*);
bool	gprs_client_transmit(sim800GprsClient_t*, uint8_t*, uint16_t);



int 	gprs_get_connection(sim800_t*, const char* mode, const char* addr, uint16_t port, uint32_t timeout);
bool	gprs_is_client_connection_ready(sim800_t*, int);

bool	gprs_client_recv(sim800_t*, int, uint8_t*, uint16_t);
bool	gprs_client_send(sim800_t*, int, uint8_t*, uint16_t);

/* callback`s */
void	on_gprs_state_callback(sim800_t*, sim800GprsState_t);
void	on_gprs_client_state_callback(sim800GprsClient_t*, sim800GprsClientState_t);
void	on_gprs_client_receive_callback(sim800GprsClient_t*, sim800GprsClientRxStage_t);
void	on_gprs_client_transmit_callback(sim800GprsClient_t*, sim800GprsClientTxStage_t);





#endif /* SIM800_GPRS_H_ */
