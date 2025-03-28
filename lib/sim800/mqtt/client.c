/*
 * MQTT client.c
 *
 *  Created on: Oct 4, 2021
 *      Author: sa100
 */

//#include "Debug.h"

#include <string.h>

#include "client.h"

/* SIM800 module */
extern sim800_t* pSim800;


/* exported variables */
mqttClient_t* primary;
mqttClient_t* reserve;


/* private data */
static mqttClient_t mqttClient[2];

/* common numeric packet ID */
static uint16_t packetId;

/* outbound buffer */
static struct {
    uint8_t data[SIM800_GPRS_PACKET_SIZE_MAX];
    uint16_t len;
} out;

/* inbound buffer */
static struct {
    uint8_t data[SIM800_GPRS_PACKET_SIZE_MAX];
    uint16_t len;
} in;



/* private function prototypes */
static void switch_state(mqttClient_t*, mqttClientState_t);
static bool is_receive_process(mqttClient_t*);
static bool is_transmit_process(mqttClient_t*);


/**
 *
 */
static void
on_data_transmit_callback(sim800GprsClient_t* conn, sim800GprsClientTxStage_t TxStage)
{
    mqttClient_t* client = (mqttClient_t*)conn->ptr;

    switch (TxStage)
    {
        case SIM800_GPRS_CLIENT_TRANSMIT_SUCCESS:
            out.len = 0;
            if (client->State == MQTT_CLIENT_STATE_PUBLISHING) {
                switch_state(client, MQTT_CLIENT_STATE_READY);
            }
            client->lastSendTimestamp = SIM800_GET_TICK();
            break;

        case SIM800_GPRS_CLIENT_TRANSMIT_FAILED:
            out.len = 0;
            if (client->State == MQTT_CLIENT_STATE_PUBLISHING) {
                switch_state(client, MQTT_CLIENT_STATE_READY);
            }
            else {
                switch_state(client, MQTT_CLIENT_STATE_UNDEFINED);
            }
            debug_printf("[ %s ] FAILED %d bytes transmit!\n", __func__, conn->Tx.len);
            break;

        default:
            break;
    }
}




static bool
is_transmit_process(mqttClient_t* client)
{
    return(client &&
           client->conn->Tx.Stage != SIM800_GPRS_CLIENT_TRANSMIT_IDLE 	 &&
           client->conn->Tx.Stage != SIM800_GPRS_CLIENT_TRANSMIT_FAILED  &&
           client->conn->Tx.Stage != SIM800_GPRS_CLIENT_TRANSMIT_SUCCESS &&
           client->conn->Tx.Stage != SIM800_GPRS_CLIENT_TRANSMIT_UNDEFINED);
}

static bool
is_receive_process(mqttClient_t* client)
{
    return(client &&
           client->conn->Rx.Stage == SIM800_GPRS_CLIENT_RECEIVE_PROCEEDING);
}

/**
 *
 */
static bool
is_expired(uint32_t timestamp, uint32_t timeout)
{
    int delta = SIM800_GET_TICK() - timestamp;
    //printf("[DELTA] %d\n", delta);

    return (delta >= timeout);
}

static bool
transmit(mqttClient_t* client, uint8_t* data, uint16_t len)
{
    bool result = false;

    if (data && len && client && client->conn &&
            gprs_client_transmit(client->conn, data, len))
    {
        debug_printf("< %s > {ts: %ld} size: %d\n",
               __func__, SIM800_GET_TICK(), len); //FIXME!

        result = true; /* SUCCESS */
    }
    return result;
}


/**
 *
 */
static void
switch_state(mqttClient_t* client, mqttClientState_t NewState)
{
    if (client) {
        switch (NewState)
        {
            case MQTT_CLIENT_STATE_UNDEFINED:
                memset(client, 0x00, sizeof(mqttClient_t)); // wipe all data
                break;

            case MQTT_CLIENT_STATE_CONNECTING:
                debug_printf("[ %s ] Registering...\n", __func__);
                client->timeout = 5 * 1000;
                client->timestamp = SIM800_GET_TICK();
                break;

            case MQTT_CLIENT_STATE_DISCONNECTING:
                debug_printf("[ %s ] Disconnecting...\n", __func__);
                client->timeout = 5 * 1000;
                client->timestamp = SIM800_GET_TICK();
                break;

            case MQTT_CLIENT_STATE_SUBSCRIBING:
                debug_printf("[ %s ] Subscribing...\n", __func__);
                client->timeout = 5 * 1000;
                client->timestamp = SIM800_GET_TICK();
                break;

            case MQTT_CLIENT_STATE_UNSUBSCRIBING:
                debug_printf("[ %s ] Unsubscribing...\n", __func__);
                client->timeout = 5 * 1000;
                client->timestamp = SIM800_GET_TICK();
                break;

            case MQTT_CLIENT_STATE_PUBLISHING:
                debug_printf("[ %s ] Publishing...\n", __func__);
                client->timeout = 5 * 1000;
                client->timestamp = SIM800_GET_TICK();
                break;


            case MQTT_CLIENT_STATE_PING:
                if (is_transmit_process(client)) {
                    /* Postpone ping... */
                    NewState = client->State;
                }
                else {
                    out.len = mqtt_packet_serialize_pingreq(out.data, sizeof(out.data));

                    if (out.len && transmit(client, out.data, out.len)) {
                        client->timestamp = SIM800_GET_TICK();
                    }
                }
                break;

            case MQTT_CLIENT_STATE_READY:
                client->timeout = client->pingPeriod;
                client->timestamp = SIM800_GET_TICK();
                break;

            case MQTT_CLIENT_STATE_ERROR:
                mqtt_client_on_disconnected_callback(client);
                memset(client->conn, 0x00, sizeof(sim800GprsClient_t));
                out.len = 0;
                in.len = 0;
                break;

            default:
                break;
        }
        client->State = NewState;
    }
}


/**
 *
 */
static void
on_connect_response(mqttClient_t* client, mqttPacket_t* packet)
{
    uint8_t ack_flags;
    uint8_t return_code;

    if (client && packet && packet->dataLen == MQTT_PACKET_CONNACK_DATA_LENGTH)
    {
        ack_flags = packet->data[0];
        return_code = packet->data[1];

        if (return_code == 0x00) {
            debug_printf("[ %s ] Connection accepted", __func__);
            debug_printf((ack_flags) ? ", clean session\n" : "\n");

            switch_state(client, MQTT_CLIENT_STATE_READY);
            mqtt_client_on_connected_callback(client);
        }
        else {
            debug_printf("[ %s ] ERROR Connection: ", __func__);
            switch (return_code)
            {
                case 0x01:
                    debug_printf("unacceptable protocol\n");
                    break;

                case 0x02:
                    debug_printf("client id rejected\n");
                    break;

                case 0x03:
                    debug_printf("server unavailable\n");
                    break;

                case 0x04:
                    debug_printf("bad user name or password\n");
                    break;

                case 0x05:
                    debug_printf("not authorized\n");
                    break;

                default:
                    debug_printf("return code = %d\n", return_code);
            }
            switch_state(client, MQTT_CLIENT_STATE_ERROR);
        }
    }
    else {
        switch_state(client, MQTT_CLIENT_STATE_ERROR);
    }
}


/**
 *
 */
static void
on_ping_response(mqttClient_t* client, mqttPacket_t* packet)
{
    if (client && packet && packet->dataLen == MQTT_PACKET_PINGRESP_DATA_LENGTH) {
        //TODO: callback
        switch_state(client, MQTT_CLIENT_STATE_READY);
    }
    else {
        switch_state(client, MQTT_CLIENT_STATE_ERROR);
    }
}


/**
 *
 */
static void
on_subscribe_response(mqttClient_t* client, mqttPacket_t* packet)
{
    uint16_t id;
    uint8_t qos;

    if (client && packet && packet->dataLen == MQTT_PACKET_SUBACK_DATA_LENGTH) {
        id = (packet->data[0] << 8) + packet->data[1];
        qos = packet->data[2];

        //printf("[MQTT] Subscribed, granted QoS: %d, packet id: %d\n", qos, id);
        //TODO: callback
        switch_state(client, MQTT_CLIENT_STATE_READY);
    }
    else {
        switch_state(client, MQTT_CLIENT_STATE_ERROR);
    }
}

/**
 *
 */
static void
on_publish_received(mqttClient_t* client, mqttPacket_t* packet)
{
    const char* topic;
    int topicLen;

    uint8_t* payload;
    int payloadSize;

    if (client && packet)
    {
        /* topic string */
        topicLen = ((packet->data[0] << 8) | packet->data[1]);
        topic = (char*)&packet->data[2];

        /* payload data */
        payloadSize = packet->dataLen - (topicLen + sizeof(uint16_t));
        payload = &packet->data[topicLen + sizeof(uint16_t)];

        if (
            topicLen <= 0 		||
			topicLen > 0xFFFF 	||
            payloadSize <= 0 	||
			payloadSize > 0xFFFF)
        {
            debug_printf("< %s > ERROR, topic length %d, payload size %d\n",
            		__func__, topicLen, payloadSize);
            return;
        }

        /* callback */
        debug_printf("< %s > {ts: %ld} size: %d\n", __func__,
               SIM800_GET_TICK(), payloadSize); //FIXME!

        mqtt_client_on_message_callback(client, topic, topicLen, payload, payloadSize);
    }
    else {
        switch_state(client, MQTT_CLIENT_STATE_ERROR);
    }
}


static int
proceed_received_packet(mqttClient_t* client, uint8_t* data, size_t len)
{
    mqttPacket_t packet;

    if (client && data && len)
    {
        mqtt_packet_deserialize(&packet, data, len);

        switch (packet.header->type)
        {
            case MQTT_PACKET_TYPE_CONNACK:
                on_connect_response(client, &packet);
                break;

            case MQTT_PACKET_TYPE_PUBLISH:
                on_publish_received(client, &packet);
                break;

//            case MQTT_PACKET_TYPE_PUBACK:
//            case MQTT_PACKET_TYPE_PUBREC:
//            case MQTT_PACKET_TYPE_PUBREL:
//            case MQTT_PACKET_TYPE_PUBCOMP:
//                Error_Handler(); //FIXME!
//                break;

            case MQTT_PACKET_TYPE_SUBACK:
                on_subscribe_response(client, &packet);
                break;

//            case MQTT_PACKET_TYPE_UNSUBACK:
//                Error_Handler(); //FIXME!
//                break;

            case MQTT_PACKET_TYPE_PINGRESP:
                on_ping_response(client, &packet);
                break;

            default:
                debug_printf("< %s > Unhandled %d bytes\n", __func__, len);
                return (-1);
                break;
        }
    }
    return packet.totalLen;
}

/**
 *
 */
static void
on_data_receive_callback(sim800GprsClient_t* conn, sim800GprsClientRxStage_t RxStage)
{
    mqttClient_t* client = (mqttClient_t*)conn->ptr;

    volatile uint8_t* ptr;
    size_t len;

    int result;

    switch (RxStage)
    {
        case SIM800_GPRS_CLIENT_RECEIVE_PENDING_DATA:
            memset(in.data, 0x00, sizeof(in.data));
            conn->Rx.ptr = in.data;
            in.len = 0;
            break;

        case SIM800_GPRS_CLIENT_RECEIVE_COMPLETE:
            //printf("[MQTT] Received %d bytes\n", conn->Rx.len);
            client->lastRecvTimestamp = SIM800_GET_TICK();

            ptr = conn->Rx.ptr;
            len = conn->Rx.len;

            //            if (len > 500) { //FIXME, debug
            //                printf("[> DEBUG <] Wow, Big one!\n");
            //            }

            /* Proceed incoming data */
            while (len)
            {
                if ((result = proceed_received_packet(client, ptr, len)) > 0)
                {
                    ptr += result;
                    len -= result;
                }
                else {
                    debug_printf("[ %s ] Decode FAILED!\n", __func__);
                    len = 0; //Error_Handler(); //FIXME!
                }
            }
            break;

        case SIM800_GPRS_CLIENT_RECEIVE_ERROR:
            debug_printf("[ %s] Receiving %d bytes FAILED!\n", __func__, conn->Rx.len);
            break;

        default:
            break;
    }

}


/**
 *
 */
mqttClientResult_t
mqtt_client_set_role(mqttClient_t* client, mqttClientRole_t Role)
{
    if (client)
    {
        if (client->State != MQTT_CLIENT_STATE_DISABLED) {
            switch_state(client, MQTT_CLIENT_STATE_DISABLED);
        }
        client->Role = Role;
        switch_state(client, MQTT_CLIENT_STATE_INITIALIZATION);
        return MQTT_CLIENT_RESULT_SUCCESS;
    }
    return MQTT_CLIENT_RESULT_ERROR;
}

/**
 *
 */
mqttClient_t*
mqtt_client_get_instance(mqttClientRole_t Role)
{
    mqttClient_t* result;

    switch (Role) {
        case MQTT_CLIENT_ROLE_PRIMARY:
            result = &mqttClient[MQTT_CLIENT_INDEX_MIN];
            break;

        default:
            result = &mqttClient[MQTT_CLIENT_INDEX_MAX];
    }
    return result;
}




/**
 *
 */
void
mqtt_client_run(mqttClient_t* client)
{
    if (client && client->State != MQTT_CLIENT_STATE_UNDEFINED)
    {
        switch (client->State)
        {
            case MQTT_CLIENT_STATE_INITIALIZATION:
                break;

            case MQTT_CLIENT_STATE_DISABLED:
                break;

            case MQTT_CLIENT_STATE_ERROR:
                break;

            default:
                if (!in.len)
                {
                    /* PING Send Timeout */
                    if (client->State == MQTT_CLIENT_STATE_READY &&
                            is_expired(client->timestamp, client->timeout))
                    {
                        //TODO: callback
                        switch_state(client, MQTT_CLIENT_STATE_PING);
                    }
                    /* PONG Receive Timeout */
                    else if (client->State == MQTT_CLIENT_STATE_PING &&
                             is_expired(client->timestamp, client->timeout))
                    {
                        //TODO: callback
                        debug_printf("[ %s ] Ping TIMEOUT!\n", __func__);
                        switch_state(client, MQTT_CLIENT_STATE_READY);
                    }
                    else {
                        /* CONNECT|SUBSCRIBE|UNSUBSCRIBE|PUBLISH Timeout */
                        if (
                            is_expired(client->timestamp, client->timeout) &&
                            (client->State == MQTT_CLIENT_STATE_CONNECTING	  ||
                             client->State == MQTT_CLIENT_STATE_DISCONNECTING ||
                             client->State == MQTT_CLIENT_STATE_SUBSCRIBING   ||
                             client->State == MQTT_CLIENT_STATE_UNSUBSCRIBING ))
                        {
                            switch_state(client, MQTT_CLIENT_STATE_UNDEFINED);
                        }
                    }
                }
                if (client->conn->State != SIM800_GPRS_CLIENT_STATE_CONNECTED) {
                    /* Total client reinitialize on GPRS Network error */
                    switch_state(client, MQTT_CLIENT_STATE_UNDEFINED);
                }
                break;
        }
    }
}

/**
 *
 */
mqttClientResult_t
mqtt_client_start(mqttClient_t* client,
                  mqttConnectData_t* connectData,
                  const char* host, uint16_t port, bool ssl)
{
    gprsClientParameters_t	gprsClientParameters =
        {
            .mode = "TCP",
            .addr = host,
            .port = port,

            .on_receive = on_data_receive_callback,
            .on_transmit = on_data_transmit_callback,

            .ptr = client,
        };

    uint32_t ts;

    if (client && connectData && host && port)
    {
        //TODO: ssl

        debug_printf("[ %s ] Connecting to '%s:%d' as '%s:%s'\n", __func__,
               host, port, connectData->username.cstr, connectData->password.cstr);

        client->conn = gprs_client_connect(pSim800, &gprsClientParameters);

        if (client->conn) {
            client->pingPeriod = (connectData->keepAlivePeriod / 2 + 1) * 1000;

            ts = SIM800_GET_TICK();
            while (client->conn->State == SIM800_GPRS_CLIENT_STATE_CONNECTING) {
                if (is_expired(ts, 5000)) {
                    return MQTT_CLIENT_RESULT_TIMEOUT;
                }
                SIM800_DELAY_MS(1);
            }

            if (client->conn->State == SIM800_GPRS_CLIENT_STATE_CONNECTED)
            {
                /* wipe input data */
                in.len = 0;

                out.len = mqtt_packet_serialize_connect(out.data, sizeof(out.data), connectData);

                if (out.len && transmit(client, out.data, out.len)) {
                    switch_state(client, MQTT_CLIENT_STATE_CONNECTING);
                    return MQTT_CLIENT_RESULT_SUCCESS;
                }
            }
        }
    }
    switch_state(client, MQTT_CLIENT_STATE_ERROR);
    return MQTT_CLIENT_RESULT_ERROR;
}


mqttClientResult_t
mqtt_client_publish_start(mqttClient_t* client)
{
    if (
        client &&
        client->conn &&
        client->conn->Tx.ptr &&
        client->conn->Tx.len &&
        transmit(client, client->conn->Tx.ptr, client->conn->Tx.len))
    {
        switch_state(client, MQTT_CLIENT_STATE_PUBLISHING);
        return MQTT_CLIENT_RESULT_SUCCESS;
    }
    return MQTT_CLIENT_RESULT_ERROR;
}


/**
 *
 */
size_t
mqtt_client_publish_prepare(mqttClient_t* client,
                            const char* topic, uint16_t topicLen,
                            void* payload, uint16_t payloadSize,
                            mqttQosLevel_t QoS, bool retain)
{
    uint8_t* ptr;
    size_t	 offset;
    size_t	 totalSize;
    size_t	 result = 0;

    if (client && client->conn && client->conn->Tx.ptr)
    {
        offset = client->conn->Tx.len;
        ptr = client->conn->Tx.ptr + offset;

        totalSize = mqtt_packet_calculate_publish_size(
                        topicLen, payloadSize, QoS);

        if (totalSize <= SIM800_GPRS_PACKET_SIZE_MAX - offset)
        {
            if (QoS > MQTT_QOS_LEVEL_0) {
                packetId = (packetId + 1) ? (packetId + 1) : 1;
            }
            result = mqtt_packet_serialize_publish(
                         ptr, SIM800_GPRS_PACKET_SIZE_MAX - offset,
                         topic, topicLen, payload, payloadSize, QoS, packetId, retain);

            client->conn->Tx.len += result;
        }
    }
    return result;
}

/**
 *
 */
mqttClientResult_t
mqtt_client_publish(const char* topic, void* payload, uint16_t len, int8_t qos, bool retain)
{
    mqttClient_t* client = mqtt_client_get_instance(MQTT_CLIENT_ROLE_PRIMARY);

    if (
        client &&
        mqtt_client_publish_prepare(client, topic, strlen(topic),
                                    payload, len, qos, retain))
    {
        return mqtt_client_publish_start(client);
    }
    return MQTT_CLIENT_RESULT_ERROR;
}


/**
 *
 */
mqttClientResult_t
mqtt_client_subscribe(const char* topic, mqttQosLevel_t QoS)
{
    mqttClient_t* client = mqtt_client_get_instance(MQTT_CLIENT_ROLE_PRIMARY);

    uint32_t ts;

    if (
        client &&
        client->conn &&
        client->conn->State == SIM800_GPRS_CLIENT_STATE_CONNECTED)
    {
        ts = SIM800_GET_TICK();
        while (client->State != MQTT_CLIENT_STATE_READY) {
            if (is_expired(ts, 1000)) {
                return MQTT_CLIENT_RESULT_TIMEOUT;
            }
            SIM800_DELAY_MS(1);
        }

        out.len = mqtt_packet_serialize_subscribe(out.data, sizeof(out.data), packetId++, 1, &topic, &QoS);

        if (out.len && transmit(client, out.data, out.len)) {
            switch_state(client, MQTT_CLIENT_STATE_SUBSCRIBING);
            return MQTT_CLIENT_RESULT_SUCCESS;
        }
    }
    return MQTT_CLIENT_RESULT_ERROR;
}


/* -------------------------------------------------------------------- */

__attribute__((weak)) void
mqtt_client_on_connected_callback(mqttClient_t* client)
{
    debug_printf("[ %s ] Client Connected\n", __func__);
}

__attribute__((weak)) void
mqtt_client_on_disconnected_callback(mqttClient_t* client)
{
    debug_printf("[ %s ] Client Disconnected!\n", __func__);
}

__attribute__((weak)) void
mqtt_client_on_message_callback(mqttClient_t* client,
                                const char* topic, uint16_t topicLen,
                                uint8_t* payload, size_t payloadSize)
{
    char str[128] = {0};

    strncpy(str, topic, topicLen);
    debug_printf("[ %s ] Message received to '%s', %d bytes\n", __func__, str, payloadSize);
}



