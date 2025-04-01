#ifndef SIM800_GSM_H_
#define SIM800_GSM_H_

#include "sim800_types.h"
#include "sim800_const.h"
#include "sim800_parser.h"


void sim800_gsm_run(Sim800Handle_t*);
void sim800_gsm_network_restart(Sim800Handle_t*);

bool sim800_sms_send(Sim800Handle_t*, char* phone, char* message);

#endif /* SIM800_GSM_H_ */
