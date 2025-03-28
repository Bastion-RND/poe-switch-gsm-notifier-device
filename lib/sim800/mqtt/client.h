/*
 * MQTT client.h
 *
 *  Created on: Oct 4, 2021
 *      Author: sa100
 */

#ifndef MQTT_CLIENT_H_
#define MQTT_CLIENT_H_

#include "sim800.h"
#include "packet.h"

#define MQTT_CLIENTS_COUNT		2
#define MQTT_CLIENT_INDEX_MIN	0
#define MQTT_CLIENT_INDEX_MAX	(MQTT_CLIENTS_COUNT - 1)


/* MQTT Client types */

typedef enum mqttClientState_
{
    MQTT_CLIENT_STATE_UNDEFINED = 0,	//< Reset state

    MQTT_CLIENT_STATE_DISABLED,			//< Client intendedly disabled

    MQTT_CLIENT_STATE_INITIALIZATION,	//< Start state (from UNDEFINED/DISABLED)
    MQTT_CLIENT_STATE_CONNECTING,		//< CONNECT sent, awaiting CONNACK
    MQTT_CLIENT_STATE_DISCONNECTING,	//< DISCONNECT sent, awaiting
    MQTT_CLIENT_STATE_SUBSCRIBING,
    MQTT_CLIENT_STATE_UNSUBSCRIBING,
    MQTT_CLIENT_STATE_PUBLISHING,
    MQTT_CLIENT_STATE_INCOMING,
	MQTT_CLIENT_STATE_PING,

	MQTT_CLIENT_STATE_READY,
    MQTT_CLIENT_STATE_ERROR,

} mqttClientState_t;

typedef enum mqttClientResult_
{
    MQTT_CLIENT_RESULT_UNDEFINED = 0,

    MQTT_CLIENT_RESULT_STARTED,
    MQTT_CLIENT_RESULT_SUCCESS,
    MQTT_CLIENT_RESULT_TIMEOUT,
    MQTT_CLIENT_RESULT_ERROR,

} mqttClientResult_t;

typedef enum mqttClientRole_ {
	MQTT_CLIENT_ROLE_UNDEFINED = 0,

	MQTT_CLIENT_ROLE_PRIMARY,
	MQTT_CLIENT_ROLE_RESERVE

} mqttClientRole_t;


#define MQTT_CLIENTID_LEN_MAX	32
#define MQTT_USERNAME_LEN_MAX	32
#define MQTT_PASSWORD_LEN_MAX	32
#define MQTT_HOSTNAME_LEN_MAX	64


typedef struct mqttClient_ mqttClient_t;
typedef void (*mqtt_client_cb_t)(mqttClient_t*, mqttClientState_t,
                                 mqttClientResult_t, void* param);


typedef struct mqttClient_
{
    sim800GprsClient_t* conn;

    uint32_t			lastSendTimestamp;
    uint32_t			lastRecvTimestamp;

    uint32_t			pingPeriod;

    uint32_t			timestamp;
    uint32_t			timeout;

    mqttClientRole_t	Role;
    mqttClientState_t	State;

} mqttClient_t;


/* exported */
extern mqttClient_t* primary;
extern mqttClient_t* reserve;


/* prototypes */
mqttClient_t*		mqtt_client_get_instance(mqttClientRole_t);
mqttClientResult_t 	mqtt_client_start(mqttClient_t*, mqttConnectData_t*, const char*, uint16_t, bool);
void 				mqtt_client_run(mqttClient_t*);

mqttClientResult_t	mqtt_client_subscribe(const char* topic, mqttQosLevel_t QoS);


size_t				mqtt_client_publish_prepare(mqttClient_t*, const char*, uint16_t, void*, uint16_t, mqttQosLevel_t, bool);
mqttClientResult_t	mqtt_client_publish_start(mqttClient_t*);
mqttClientResult_t 	mqtt_client_publish(const char*, void*, uint16_t, int8_t, bool);


/* callbacks */
void mqtt_client_on_connected_callback(mqttClient_t*);
void mqtt_client_on_disconnected_callback(mqttClient_t*);
void mqtt_client_on_message_callback(mqttClient_t*, const char*, uint16_t, uint8_t*, size_t);


#endif /* MQTT_CLIENT_H_ */
