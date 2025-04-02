#ifndef SIM800_TYPES_H_
#define SIM800_TYPES_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "circular_buffer.h"

#include "sim800_gsm_types.h"

typedef enum sim800_Flow_ {
    SIM800_FLOW_ASYNC = 0U,
    SIM800_FLOW_SYNC = !SIM800_FLOW_ASYNC,
} sim800_Flow_t;

typedef enum Sim800Result_ {
    SIM800_RESULT_OK = 0,
    SIM800_RESULT_ERROR,
    SIM800_RESULT_TIMEOUT,
} Sim800Result_t;

typedef enum Sim800ModuleState_ {
    SIM800_MODULE_STATE_UNDEFINED = 0,
    SIM800_MODULE_STATE_DISABLED,
    SIM800_MODULE_STATE_INITIALIZATION,
    SIM800_MODULE_STATE_NOT_REGISTERED,
    SIM800_MODULE_STATE_READY,
    SIM800_MODULE_STATE_ERROR,
} Sim800ModuleState_t;

typedef enum Sim800State_ {
    SIM800_STATE_UNDEFINED = 0,
    SIM800_STATE_INIT_MODULE,
    SIM800_INIT_GSM_NETWORK,
    SIM800_INIT_SMS_LAYER,
    SIM800_STATE_READY,
    SIM800_STATE_ERROR,
} Sim800State_t;

typedef enum Sim800Event_ {
    SIM800_EVENT_CMD_RESULT_OK = 0,
    SIM800_EVENT_CMD_RESULT_ERR,
    SIM800_EVENT_CMD_RESULT_TIMEOUT,
    SIM800_EVENT_TIMER_REACHED,
    SIM800_EVENT_TRANSMIT_SUCCESS,
    SIM800_EVENT_TRANSMIT_ERROR,
    SIM800_EVENT_TRANSMIT_TIMEOUT,
    SIM800_EVENT_INITIALIZATION_BEGIN,
} Sim800Event_t;

typedef struct Sim800Handle_ Sim800Handle_t;

typedef void (*sim800_parser_handler_t)(Sim800Handle_t*, const char *, void *param);

typedef void (*sim800_callback_t)(Sim800Handle_t*, Sim800Event_t, void *param);

typedef struct Sim800Parser_ {
    const char *str;
    size_t len;
    sim800_parser_handler_t handler;
    void *handler_param;
} Sim800Parser_t;

typedef struct sim800_TxBuffer_ {
    uint8_t *pData;
    volatile size_t index;
    volatile size_t size;
} sim800_TxBuffer_t;

typedef struct Sim800Timer_ {
    bool active;
    uint32_t timestamp;
    uint32_t timeout_ms;
    sim800_callback_t callback;
} Sim800Timer_t;

typedef enum Sim800SimCardState_ {
    SIM800_SIM_CARD_READY = 0,
    SIM800_SIM_CARD_PIN,
    SIM800_SIM_CARD_UNDEFINED,
} Sim800SimCardState_t;

typedef struct Sim800Module_ {
    char model[32];
    char revision[32];
    char serialNumber[32];
    Sim800SimCardState_t SimCardState;
    Sim800ModuleState_t State;
    struct {
        int raw;
        int dBm;
    } RSSI;
    Sim800Timer_t Timer;
} Sim800Module_t;

typedef struct Sim800Handle_ {
    struct {
        bool _mutex;
        uint32_t ts;
        char str[128];
        uint32_t timeout;
        uint32_t attemptCounter;
        sim800_callback_t callback;
        void *callback_param;
    } Command;

    cbuf_handle_t TxCbufHandle;
    cbuf_handle_t RxCbufHandle;
    Sim800Module_t Module;
    Sim800Gsm_t Gsm;
    Sim800Parser_t* ParsersList;
} Sim800Handle_t;

#endif /* SIM800_TYPES_H_ */
