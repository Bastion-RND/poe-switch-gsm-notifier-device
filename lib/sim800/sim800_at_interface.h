#ifndef SIM800_AT_INTERFACE_H
#define SIM800_AT_INTERFACE_H

#include "main.h"

typedef enum Sim800AtInterfaceEvent_ {
    SIM800_AT_INTERFACE_EVENT_RESPONSE_OK = 0,
    SIM800_AT_INTERFACE_EVENT_RESPONSE_ERROR,
    SIM800_AT_INTERFACE_EVENT_RESPONSE_TIMEOUT,
} Sim800AtInterfaceEvent_t;

bool sim800_send_at_command(char *cmd, char *response, uint32_t timeout);

#endif //SIM800_AT_INTERFACE_H
