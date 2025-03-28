#ifndef SIM800_GSM_H_
#define SIM800_GSM_H_

#include "sim800_types.h"
#include "sim800_const.h"
#include "sim800_parser.h"


void sim800_gsm_run(sim800_t*);

bool sim800_call_answer(sim800_t*);
bool sim800_call_hangup(sim800_t*);
bool sim800_sms_send(sim800_t* p, char* phone, char* message);
bool sim800_ussd_request(sim800_t*, char* ussd);

void on_gsm_state_callback(sim800_t*, sim800GsmState_t);
void on_gsm_ring_callback(sim800_t*, sim800GsmCall_t*);
void on_gsm_call_callback(sim800_t*, sim800GsmCall_t*);
void on_gsm_dtmf_callback(sim800_t*, sim800GsmDtmf_t*);
void on_gsm_ussd_callback(sim800_t*, const char*, int);

#endif /* SIM800_GSM_H_ */
