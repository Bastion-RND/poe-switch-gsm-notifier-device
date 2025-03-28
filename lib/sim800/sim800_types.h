/*
 * sim800_types.h
 *
 *  Created on: Sep 25, 2021
 *      Author: sa100
 */

#ifndef SIM800_TYPES_H_
#define SIM800_TYPES_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

//#include "rbuffer.h"
#include "circular_buffer.h"

#include "sim800_gsm_types.h"
#include "sim800_gprs_types.h"


typedef enum sim800_Flow_
{
	SIM800_FLOW_ASYNC	= 0U,
	SIM800_FLOW_SYNC	= !SIM800_FLOW_ASYNC,

} sim800_Flow_t;


typedef enum sim800_Result_
{
    SIM800_RESULT_OK = 0,
    SIM800_RESULT_ERROR,
    SIM800_RESULT_TIMEOUT,

} sim800_Result_t;


typedef enum sim800_ModuleState_
{
    SIM800_STATE_MODULE_UNDEFINED = 0,

	SIM800_STATE_MODULE_DISABLED,
    SIM800_STATE_MODULE_INITIALIZATION,
    SIM800_STATE_MODULE_READY,
    SIM800_STATE_MODULE_ERROR,

} sim800_ModuleState_t;



typedef enum sim800_Event_
{
    SIM800_EVENT_UNDEFINED = 0,
    SIM800_EVENT_COMMAND_RESULT_OK,
    SIM800_EVENT_COMMAND_RESULT_ERROR,
    SIM800_EVENT_COMMAND_RESULT_TIMEOUT,

	SIM800_EVENT_TRANSMIT_TIMEOUT,
	SIM800_EVENT_TRANSMIT_SUCCESS,
	SIM800_EVENT_TRANSMIT_ERROR,

	SIM800_EVENT_INITIALIZATION_BEGIN,
	SIM800_EVENT_INITIALIZATION_COMPLETE,

} sim800_Event_t;



typedef struct sim800_ sim800_t;

typedef void (*sim800_parser_handler_t)(sim800_t*, const char*, void* param);
typedef void (*sim800_callback_t)(sim800_t*, sim800_Event_t, void* param);

//typedef void (*sim800_at_cmd_handler_t)(sim800_t*, const char*, void* param);


typedef struct sim800_Parser_
{
    const char* 				str;
    size_t						len;
    sim800_parser_handler_t 	handler;
    void*						handler_param;

} sim800_Parser_t;


typedef struct sim800_TxBuffer_
{
    uint8_t*					pData;
    volatile size_t				index;
    volatile size_t				size;

} sim800_TxBuffer_t;

typedef struct sim800_Module_
{
    char						model[32];
    char						revision[32];
    char						serialNumber[32];

    struct {
        int						raw;
        int						dBm;
    } RSSI;

    sim800_ModuleState_t		State;

} sim800_Module_t;

struct sim800_
{
    sim800Gsm_t					Gsm;
    sim800Gprs_t				Gprs;
    sim800_Module_t				Module;


    sim800_Parser_t* 			Parser;

    struct {
        bool					_mutex;

        uint32_t				ts;

        char*					str;
        uint32_t				timeout;

        sim800_callback_t		callback;
        void*					callback_param;

    } Command;

    cbuf_handle_t				TxCbufHandle;
    cbuf_handle_t				RxCbufHandle;
};


#endif /* SIM800_TYPES_H_ */
