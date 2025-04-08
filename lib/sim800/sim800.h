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

#ifndef CMD_MAX_ATTEMPT
#define CMD_MAX_ATTEMPT 20
#endif

#ifndef REPEAT_CMD_TIMEOUT_MS
#define REPEAT_CMD_TIMEOUT_MS 1000
#endif

Sim800Handle_t* sim800_init(void);
void sim800_run(Sim800Handle_t*);
void sim800_uart_handler(Sim800Handle_t*);
bool sim800_parser_add(Sim800Handle_t*, const char *, sim800_parser_handler_t, void *param);
bool sim800_parser_remove(Sim800Handle_t*, const char *);
Sim800Result_t sim800_cmd(Sim800Handle_t*, const char *, uint32_t to, sim800_callback_t, void *param, sim800_Flow_t);
void sim800_unlock(Sim800Handle_t*);
bool sim800_is_locked(Sim800Handle_t*);
void on_pin_checked_callback(const char *status);
void on_rssi_updated_callback(int dBm);
void on_new_sms_callback(char *ptrPhoneNum, char *ptrTxt);

#endif /* SIM800_H_ */
