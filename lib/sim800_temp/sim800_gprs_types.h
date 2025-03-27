/*
 * types.h
 *
 *  Created on: Sep 28, 2021
 *      Author: sa100
 */

#ifndef SIM800_GPRS_TYPES_H_
#define SIM800_GPRS_TYPES_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


#define GPRS_CLIENTS_COUNT 		6
#define GPRS_CLIENTS_INDEX_MIN 	0
#define GPRS_CLIENTS_INDEX_MAX 	(GPRS_CLIENTS_COUNT - 1)

#define GPRS_CLIENT_INVALID		(-1)


typedef enum sim800GprsState_
{
    SIM800_GPRS_STATE_IP_INITIAL 	= 0,
    SIM800_GPRS_STATE_IP_START		= 1,
    SIM800_GPRS_STATE_IP_CONFIG		= 2,
    SIM800_GPRS_STATE_IP_GPRSACT	= 3,
    SIM800_GPRS_STATE_IP_STATUS		= 4,
    SIM800_GPRS_STATE_IP_PROCESSING = 5,
	SIM800_GPRS_STATE_CONNECTED 	= 6,
	SIM800_GPRS_STATE_CLOSING 		= 7,
	SIM800_GPRS_STATE_CLOSED 		= 8,
    SIM800_GPRS_STATE_PDP_DEACT		= 9,

    SIM800_GPRS_STATE_INITIALIZATION,
	SIM800_GPRS_STATE_DISABLED,
    SIM800_GPRS_STATE_READY,
	SIM800_GPRS_STATE_ERROR,

} sim800GprsState_t;


typedef enum sim800GprsClientState_
{
    SIM800_GPRS_CLIENT_STATE_UNDEFINED = 0,

    SIM800_GPRS_CLIENT_STATE_INITIAL,
    SIM800_GPRS_CLIENT_STATE_CONNECTING,
    SIM800_GPRS_CLIENT_STATE_CONNECTED,
    SIM800_GPRS_CLIENT_STATE_REMOTE_CLOSING,
    SIM800_GPRS_CLIENT_STATE_CLOSING,
    SIM800_GPRS_CLIENT_STATE_CLOSED,
    SIM800_GPRS_CLIENT_STATE_FAILED,

} sim800GprsClientState_t;


typedef enum sim800GprsClientTxStage_
{
    SIM800_GPRS_CLIENT_TRANSMIT_UNDEFINED = 0,

    SIM800_GPRS_CLIENT_TRANSMIT_PENDING_DATA,
    SIM800_GPRS_CLIENT_TRANSMIT_AWAITING_PROMPT,
    SIM800_GPRS_CLIENT_TRANSMIT_PROCEEDING,
	SIM800_GPRS_CLIENT_TRANSMIT_AWAITING_RESULT,
    SIM800_GPRS_CLIENT_TRANSMIT_SUCCESS,
    SIM800_GPRS_CLIENT_TRANSMIT_FAILED,
    SIM800_GPRS_CLIENT_TRANSMIT_IDLE,

} sim800GprsClientTxStage_t;


typedef enum sim800GprsClientRxStage_
{
    SIM800_GPRS_CLIENT_RECEIVE_UNDEFINED = 0,

    SIM800_GPRS_CLIENT_RECEIVE_PENDING_DATA,
    SIM800_GPRS_CLIENT_RECEIVE_PROCEEDING,
    SIM800_GPRS_CLIENT_RECEIVE_COMPLETE,
    SIM800_GPRS_CLIENT_RECEIVE_ERROR,
    SIM800_GPRS_CLIENT_RECEIVE_IDLE,

} sim800GprsClientRxStage_t;

typedef union sim800_IpAddress_
{
    uint8_t		u8[4];
    uint32_t	u32;

} sim800IpAddress_t;


typedef struct sim800GprsClient_ sim800GprsClient_t;
typedef void (*onGprsClientTransmit_cb)(sim800GprsClient_t*, sim800GprsClientTxStage_t);
typedef void (*onGprsClientReceive_cb)(sim800GprsClient_t*, sim800GprsClientRxStage_t);


typedef struct gprsClientParameters_
{
	void*							ptr;

	const char* 					mode;
	const char*						addr;
	uint16_t						port;

	onGprsClientTransmit_cb			on_transmit;
	onGprsClientReceive_cb			on_receive;

} gprsClientParameters_t;


typedef struct sim800GprsClient_
{
    void*							ptr;

    char							mode[4];
    char							addr[64];
    int								port;

    sim800IpAddress_t				IP;

    struct {
        volatile uint8_t*			ptr;
        volatile uint16_t			idx;
        volatile uint16_t			len;
        sim800GprsClientTxStage_t 	Stage;
        onGprsClientTransmit_cb		callback;
        uint32_t					timestamp;
    } Tx;

    struct {
        volatile uint8_t*			ptr;
        volatile uint16_t			idx;
        volatile uint16_t			len;
        sim800GprsClientRxStage_t 	Stage;
        onGprsClientReceive_cb		callback;
        uint32_t					timestamp;
    } Rx;

    sim800GprsClientState_t			State;

} sim800GprsClient_t;


typedef struct sim800Gprs_
{
    sim800IpAddress_t				IP;

    sim800GprsClient_t				Client[GPRS_CLIENTS_COUNT];

    sim800GprsState_t				State;

} sim800Gprs_t;


#endif /* SIM800_GPRS_TYPES_H_ */
