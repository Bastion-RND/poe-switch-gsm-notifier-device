/*
 * MQTT packet.h
 *
 *  Created on: Oct 9, 2021
 *      Author: sa100
 */

#ifndef MQTT_PACKET_H_
#define MQTT_PACKET_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MQTT_PACKET_CONNACK_DATA_LENGTH		2
#define MQTT_PACKET_SUBACK_DATA_LENGTH		3
#define MQTT_PACKET_PINGRESP_DATA_LENGTH	0

#define MQTT_3_1_MAGIC_WORD					"MQIsdp"
#define MQTT_3_1_1_MAGIC_WORD				"MQTT"

/* types */
typedef enum mqttVersion_
{
    MQTT_VERSION_3_1   = 3, // 3.1
    MQTT_VERSION_3_1_1 = 4  // 3.1.1

} mqttVersion_t;

typedef enum mqttQosLevel_
{
    MQTT_QOS_LEVEL_0 = 0, // At most once
    MQTT_QOS_LEVEL_1 = 1, // At least once
    MQTT_QOS_LEVEL_2 = 2  // Exactly once

} mqttQosLevel_t;

typedef enum mqttPacketType_
{
    MQTT_PACKET_TYPE_UNDEFINED = 0,
    MQTT_PACKET_TYPE_CONNECT,
    MQTT_PACKET_TYPE_CONNACK,
    MQTT_PACKET_TYPE_PUBLISH,
    MQTT_PACKET_TYPE_PUBACK,
    MQTT_PACKET_TYPE_PUBREC,
    MQTT_PACKET_TYPE_PUBREL,
    MQTT_PACKET_TYPE_PUBCOMP,
    MQTT_PACKET_TYPE_SUBSCRIBE,
    MQTT_PACKET_TYPE_SUBACK,
    MQTT_PACKET_TYPE_UNSUBSCRIBE,
    MQTT_PACKET_TYPE_UNSUBACK,
    MQTT_PACKET_TYPE_PINGREQ,
    MQTT_PACKET_TYPE_PINGRESP,
    MQTT_PACKET_TYPE_DISCONNECT,

} mqttPacketType_t;

#pragma pack(push, 1)

typedef struct mqttString_
{
	uint16_t	len;
	char*		cstr;

} mqttString_t;


typedef union mqttConnectFlags_
{
	struct {
		uint8_t unused		 : 1;
		uint8_t cleanSession : 1; // clean session flag
		uint8_t will		 : 1; // will message set flag
		uint8_t willQos		 : 2; // will QoS value
		uint8_t willRetain	 : 1; // will retain flag
		uint8_t password	 : 1; // password flag
		uint8_t username	 : 1; // username flag
	};
	uint8_t		u8;

} mqttConnectFlags_t;


typedef struct mqttWillOptions_
{
	mqttString_t 	topic;
	mqttString_t 	message;

} mqttWillOptions_t;


typedef struct mqttConnectData_
{
	mqttVersion_t		protocol;
	mqttConnectFlags_t	flags;
	uint16_t			keepAlivePeriod;
	mqttString_t		clientId;
	mqttWillOptions_t	will;
	mqttString_t		username;
	mqttString_t		password;

} mqttConnectData_t;

typedef union mqttPacketHeader_ {
	struct {
		uint8_t retain		: 1; // retained flag bit
		uint8_t qos			: 2; // QoS value, 0, 1 or 2
		uint8_t dup			: 1; // DUP flag bit
		uint8_t	type		: 4; // message type nibble
	};
	uint8_t		u8;
} mqttPacketHeader_t;

typedef struct mqttPacket_
{
	mqttPacketHeader_t*	header;
	uint8_t*			data;
	size_t				dataLen;
	size_t				totalLen;

} mqttPacket_t;

#pragma pack(pop)


/* prototypes */
//void	mqtt_packet_decode_header(mqttPacketHeader_t*, uint8_t**, size_t*);
//size_t	mqtt_packet_get_packet_length(uint8_t**, size_t*);

void	mqtt_packet_deserialize(mqttPacket_t*, uint8_t*, size_t);
size_t	mqtt_packet_serialize_connect(uint8_t*, size_t, mqttConnectData_t*);
size_t	mqtt_packet_serialize_disconnect(uint8_t*, size_t);
size_t	mqtt_packet_serialize_subscribe(uint8_t*, size_t, uint16_t, int, const char* topic[], mqttQosLevel_t Qos[]);
size_t	mqtt_packet_serialize_unsubscribe(uint8_t*, size_t, uint16_t, int, const char* topic[]);
size_t	mqtt_packet_serialize_publish(uint8_t*, size_t, const char*, size_t, void*, size_t, mqttQosLevel_t, uint16_t, bool);
size_t	mqtt_packet_calculate_publish_size(uint16_t topicLen, size_t payloadLen, mqttQosLevel_t QoS);
size_t	mqtt_packet_serialize_pingreq(uint8_t*, size_t);


#endif /* MQTT_PACKET_H_ */
