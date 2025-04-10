#ifndef SIM800_GSM_H_
#define SIM800_GSM_H_

#include <stddef.h>

#include "sim800_types.h"
#include "sim800_const.h"
#include "sim800_parser.h"

#define SINGLE_SMS_LENGTH_MAX     (size_t)256


void   sim800_gsm_run(Sim800Handle_t*);
void   sim800_gsm_network_restart(Sim800Handle_t*);
bool   sim800_sms_send(Sim800Handle_t*, char* phone, char* message, sim800_sms_callback_t cb);

#endif /* SIM800_GSM_H_ */
