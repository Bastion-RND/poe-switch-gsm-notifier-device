/*
 * MQTT packet.c
 *
 *  Created on: Oct 9, 2021
 *      Author: sa100
 */

#include <string.h>
#include "packet.h"


static size_t
mqttString_len(mqttString_t* mqttStr)
{
    if (mqttStr && mqttStr->cstr)
    {
        return ((mqttStr->len) ? mqttStr->len : strlen(mqttStr->cstr));
    }
    return 0;
}

static size_t
encode_packet_len(uint8_t** pptr, size_t len)
{
    size_t bytes = 0;

    if (pptr && *pptr && len)
    {
        do {
            uint8_t d = len % 128;
            len /= 128;

            if (len) {
                d |= 0x80;
            }
            **pptr = d;
            *pptr += 1;

            bytes++;

        } while (len);
    }
    return bytes;
}

static size_t
encode_bytes(uint8_t** pptr, void* data, size_t len)
{
    int bytes = 0;

    if (pptr && *pptr && data && len)
    {
        while (bytes < len)
        {
            **pptr = *(uint8_t*)data;

            *pptr += 1;
            data += 1;

            bytes++;
        }
    }
    return bytes;
}

static size_t
encode_mqtt_string(uint8_t** pptr, mqttString_t* mqttStr)
{
    int index = 0;
    uint16_t len;

    if (pptr && *pptr && mqttStr && mqttStr->cstr)
    {
        len = mqttString_len(mqttStr);

        /* encode string length */
        (*pptr)[0] = len >> 0x8;
        (*pptr)[1] = len & 0xFF;
        *pptr += 2;

        /* encode string data */
        while (index < len)
        {
            **pptr = (uint8_t)mqttStr->cstr[index];

            *pptr += 1;
            index += 1;
        }
    }
    return index;
}

/**
 *
 */
void
mqtt_packet_deserialize(mqttPacket_t* packet,
                        uint8_t* buf, size_t bufSize)
{
    int multiplyer = 1;
    uint8_t* ptr;

    if (packet && buf && bufSize) {
        memset(packet, 0x00, sizeof(mqttPacket_t));
        packet->header = (mqttPacketHeader_t*)buf;

        if (
            packet->header->type >= MQTT_PACKET_TYPE_CONNECT &&
            packet->header->type <= MQTT_PACKET_TYPE_DISCONNECT)
        {
            packet->totalLen = 1;
            packet->dataLen = 0;
            ptr = buf;

            do {
            	ptr++;
                packet->dataLen += (*ptr & 0x7F) * multiplyer;
                packet->totalLen += 1;
                multiplyer *= 0x80;

            } while ((*ptr & 0x80) && (packet->totalLen <= bufSize));

            packet->totalLen += packet->dataLen;
            packet->data = &ptr[1]; // skip last size byte
        }
    }
}

/**
 *
 */
size_t
mqtt_packet_serialize_connect(uint8_t* buf,
                              size_t bufSize,
                              mqttConnectData_t* connectData)
{
    mqttPacketHeader_t* header;
    mqttConnectFlags_t* flags;

    size_t dataLen;
    size_t totalLen;

    mqttString_t magic = {0};

    uint8_t* ptr = buf;

    if (
        buf && bufSize &&
        connectData->clientId.cstr &&
        connectData->protocol >= MQTT_VERSION_3_1 &&
        connectData->protocol <= MQTT_VERSION_3_1_1)
    {
        /* calculate data length */
        if (connectData->protocol == MQTT_VERSION_3_1_1) {
            magic.cstr = "MQTT";
        }
        else{
            magic.cstr = "MQIsdp";
        }
        dataLen = mqttString_len(&magic) + sizeof(uint16_t) + sizeof(uint8_t);
        dataLen += sizeof(mqttConnectFlags_t);
        dataLen += sizeof(connectData->keepAlivePeriod);
        dataLen += mqttString_len(&connectData->clientId) + sizeof(uint16_t);

        if (connectData->flags.will) {
            dataLen += mqttString_len(&connectData->will.topic) + sizeof(uint16_t);
            dataLen += mqttString_len(&connectData->will.message) + sizeof(uint16_t);
        }

        if (connectData->username.cstr) {
            dataLen += mqttString_len(&connectData->username) + sizeof(uint16_t);
        }
        if (connectData->password.cstr) {
            dataLen += mqttString_len(&connectData->password) + sizeof(uint16_t);
        }

        /* packet header */
        header = (mqttPacketHeader_t*)ptr++;
        header->u8 = 0x00;
        header->type = MQTT_PACKET_TYPE_CONNECT;

        /* packet length */
        totalLen = sizeof(mqttPacketHeader_t);
        totalLen += encode_packet_len(&ptr, dataLen) + dataLen;

        if (totalLen <= bufSize)
        {
            /* MQTT protocol version */
            encode_mqtt_string(&ptr, &magic);
            if (connectData->protocol == MQTT_VERSION_3_1_1) {
                ptr[0] = 0x04;
            }
            else{
                ptr[0] = 0x03;
            }
            ptr += 1;

            /* connection flags */
            flags = (mqttConnectFlags_t*)ptr++;
            flags->u8 = connectData->flags.u8;

            /* keep alive period */
            ptr[0] = connectData->keepAlivePeriod >> 0x8;
            ptr[1] = connectData->keepAlivePeriod & 0xFF;
            ptr += 2;

            /* client ID */
            encode_mqtt_string(&ptr, &connectData->clientId);

            /* last will message */
            if (flags->will)
            {
                encode_mqtt_string(&ptr, &connectData->will.topic);
                encode_mqtt_string(&ptr, &connectData->will.message);
            }

            /* User name */
            if (connectData->username.cstr) {
                flags->username = true;
                encode_mqtt_string(&ptr, &connectData->username);
            }

            /* Password */
            if (connectData->password.cstr) {
                flags->password = true;
                encode_mqtt_string(&ptr, &connectData->password);
            }
        }
        return totalLen;
    }
    return 0; /* ERROR */
}

/**
 *
 */
size_t
mqtt_packet_serialize_disconnect(uint8_t* buf,
                                 size_t bufSize)
{
    mqttPacketHeader_t* header;
    const size_t totalLen = 2;

    uint8_t* ptr = buf;

    if (buf && bufSize && bufSize >= totalLen)
    {
        /* packet header */
        header = (mqttPacketHeader_t*)ptr++;
        header->u8 = 0x00;
        header->type = MQTT_PACKET_TYPE_DISCONNECT;

        /* packet data length */
        *ptr = 0x00;

        return totalLen;
    }
    return 0; /* ERROR */
}

/**
 *
 */
size_t
mqtt_packet_serialize_pingreq(uint8_t* buf,
                              size_t bufSize)
{
    mqttPacketHeader_t* header;
    const size_t totalLen = 2;

    uint8_t* ptr = buf;

    if (buf && bufSize && bufSize >= totalLen)
    {
        /* packet header */
        header = (mqttPacketHeader_t*)ptr++;
        header->u8 = 0x00;
        header->type = MQTT_PACKET_TYPE_PINGREQ;

        /* packet data length */
        *ptr = 0x00;

        return totalLen;
    }
    return 0; /* ERROR */
}

/**
 *
 */
size_t
mqtt_packet_serialize_subscribe(uint8_t* buf,
                                size_t bufSize,
                                uint16_t packetId,
                                int subscribeCount,
                                const char* topic[],
                                mqttQosLevel_t QoS[])
{
    mqttPacketHeader_t* header;
    mqttString_t topicString = {0};

    size_t dataLen;
    size_t totalLen;

    uint8_t* ptr = buf;

    if (buf && bufSize && topic)
    {
        /* calculate data length */
        dataLen = sizeof(packetId);
        for (int i = 0; i < subscribeCount; i++) {
            topicString.cstr = (char*)topic[i];
            dataLen += mqttString_len(&topicString) + sizeof(uint16_t);
            dataLen += sizeof(uint8_t); // QoS
        }

        /* packet header */
        header = (mqttPacketHeader_t*)ptr++;
        header->u8 = 0x02; // NOTE! SUBSCRIBE|UNSUBSCRIBE|PUBREL
        header->type = MQTT_PACKET_TYPE_SUBSCRIBE;

        /* packet length */
        totalLen = sizeof(mqttPacketHeader_t);
        totalLen += encode_packet_len(&ptr, dataLen) + dataLen;

        if (totalLen <= bufSize)
        {
            /* encode packetId */
            ptr[0] = packetId >> 0x8;
            ptr[1] = packetId & 0xFF;
            ptr += 2;

            for (int i = 0; i < subscribeCount; i++) {
                topicString.cstr = (char*)topic[i];
                encode_mqtt_string(&ptr, &topicString);
                ptr[0] = QoS[i];
                ptr += 1;
            }
        }
        return totalLen;
    }
    return 0; /* ERROR */
}


size_t
mqtt_packet_calculate_publish_size(uint16_t topicLen, size_t payloadLen, mqttQosLevel_t QoS)
{
    size_t totalLen = sizeof(mqttPacketHeader_t);
    size_t dataLen = sizeof(topicLen) + topicLen + payloadLen;

    uint8_t data[4] = { 0 };
    uint8_t* ptr = data;

    if (QoS > MQTT_QOS_LEVEL_0) {
        dataLen += sizeof(uint16_t /* packetId */);
    }

    totalLen += encode_packet_len(&ptr, dataLen) + dataLen;
    return totalLen;
}


/**
 *
 */
size_t
mqtt_packet_serialize_publish(uint8_t* buf,
                              size_t bufSize,
                              const char* topic, size_t topicLen,
                              void* payload, size_t payloadLen,
                              mqttQosLevel_t QoS,
                              uint16_t packetId,
                              bool retained)
{
    mqttPacketHeader_t* header;
    mqttString_t topicString =
        {
            .cstr = (char*)topic,
            .len = topicLen,
        };
    size_t dataLen;
    size_t totalLen;

    uint8_t* ptr = buf;

    if (buf && bufSize && topic)
    {
        /* calculate data length */
        dataLen = mqttString_len(&topicString) + sizeof(uint16_t) + payloadLen;
        if (QoS > MQTT_QOS_LEVEL_0) {
            dataLen += sizeof(packetId);
        }

        /* packet header */
        header = (mqttPacketHeader_t*)ptr++;
        header->u8 = 0x00;
        header->qos = QoS;
        header->retain = retained;
        header->type = MQTT_PACKET_TYPE_PUBLISH;

        /* packet length */
        totalLen = sizeof(mqttPacketHeader_t);
        totalLen += encode_packet_len(&ptr, dataLen) + dataLen;

        if (totalLen <= bufSize)
        {
            encode_mqtt_string(&ptr, &topicString);

            if (QoS > MQTT_QOS_LEVEL_0) {
                /* encode packetId */
                ptr[0] = packetId >> 0x8;
                ptr[1] = packetId & 0xFF;
                ptr += 2;
            }

            encode_bytes(&ptr, payload, payloadLen);
        }
        return totalLen;
    }
    return 0; /* ERROR */
}


//
//size_t
//mqtt_packet_get_packet_length(uint8_t** ptr, size_t* remaining_len)
//{
//    size_t packet_len = 0;
//    int multiplyer = 1;
//
//    if (ptr && *remaining_len) {
//        do {
//            packet_len += (**ptr & 0x7f) * multiplyer;
//            multiplyer *= 0x80;
//
//            *ptr += 1;
//            *remaining_len -= 1;
//
//        } while ((**ptr & 0x80) && *remaining_len);
//    }
//    return packet_len;
//}
