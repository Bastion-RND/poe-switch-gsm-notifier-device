/*
 * sim800_gsm.h
 *
 *  Created on: Sep 25, 2021
 *      Author: sa100
 */

#ifndef SIM800_GSM_H_
#define SIM800_GSM_H_

#include "sim800_types.h"
#include "sim800_const.h"
#include "sim800_parser.h"



/* prototypes */
void sim800_gsm_run(sim800_t*);

bool sim800_call_answer(sim800_t*);
bool sim800_call_hangup(sim800_t*);
bool sim800_sms_send(sim800_t* p, char* phone, char* message);
bool sim800_ussd_request(sim800_t*, char* ussd);

/* callback`s ----------------------------------------------- */
void on_gsm_state_callback(sim800_t*, sim800GsmState_t);

/* Call related */
void on_gsm_ring_callback(sim800_t*, sim800GsmCall_t*);
void on_gsm_call_callback(sim800_t*, sim800GsmCall_t*);
void on_gsm_dtmf_callback(sim800_t*, sim800GsmDtmf_t*);

/* USSD related */
void on_gsm_ussd_callback(sim800_t*, const char*, int);

#endif /* SIM800_GSM_H_ */
