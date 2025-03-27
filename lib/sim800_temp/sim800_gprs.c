/*
 * sim800_gprs.c
 *
 *  Created on: Sep 28, 2021
 *      Author: sa100
 */

#include <string.h>

//#include "Debug.h"

#include "sim800.h"


/* internal function prototypes */
static void switch_gprs_state(sim800_t*, sim800GprsState_t);
static void switch_client_state(sim800GprsClient_t*, sim800GprsClientState_t);
static void switch_client_rx_stage(sim800GprsClient_t*, sim800GprsClientRxStage_t);
static void switch_client_tx_stage(sim800GprsClient_t*, sim800GprsClientTxStage_t);

static void	gprs_init_process(sim800_t*, sim800_Event_t, void*);

//static void prompt_parser(sim800_t*, void*);
//static void receive_parser(sim800_t*, void*);


/* internal functions -------------------------------------------- */

/**
 *
 */
static sim800GprsClient_t*
get_client_by_index(sim800_t* p, int index)
{
    if (
        index >= GPRS_CLIENTS_INDEX_MIN &&
        index <= GPRS_CLIENTS_INDEX_MAX && p)
    {
        return &p->Gprs.Client[index];
    }
    return NULL;
}


/* command callback`s --------------------------------------------- */

/**
 *
 */
static void
request_cipstatus(sim800_t* p, sim800_Event_t ev, void* param)
{
    sim800_callback_t cb = param;

    if (cb) {
        if (ev == SIM800_EVENT_COMMAND_RESULT_OK) {
            sim800_cmd(p, "AT+CIPSTATUS\n", 1000, cb, NULL, SIM800_FLOW_ASYNC);
        }
        else {
            gprs_init_process(p, ev, NULL);
        }
    }
    else {
        sim800_cmd(p, "AT+CIPSTATUS\n", 1000, NULL, NULL, SIM800_FLOW_ASYNC);
    }
}



/* reply parsers -------------------------------------------------- */

static void
cipshut_parser(sim800_t* p, const char* str, void* param)
{
    sim800_parser_remove(p, GPRS_SHUT_OK);

    _sim800_cmd_unlock(p); /* NOTE! it`s OK */
    gprs_init_process(p, SIM800_EVENT_COMMAND_RESULT_OK, NULL);
}

static void
cifsr_parser(sim800_t* p, const char* str, void* param)
{
    sim800IpAddress_t* ip = (sim800IpAddress_t*)param;

    char source[32];
    sim800_readline(p, source, sizeof(source), 1000);

    //FIXME! sim800_ip_addr_parse(p, &ip->u8[0]);
    sim800_parser_remove(p, RESPONSE_LOCAL_IP);

    _sim800_cmd_unlock(p); /* NOTE! No OK\r\n */
    request_cipstatus(p, SIM800_EVENT_COMMAND_RESULT_OK, NULL);
}



/**
 *
 */
static void
client_state_parser(sim800_t* p, const char* str, void* param)
{
    const char* ptr = str + strlen(C_STATE_RESPONSE);

    int index, bearer;
    char tmp[16];

    sim800GprsClient_t* client;

    index = sim800_parse_int(&ptr);

    if ((client = get_client_by_index(p, index)))
    {
        bearer = sim800_parse_int(&ptr);

        sim800_parse_str(&ptr, &client->mode[0]);
        ptr += 2; /* skip [ ," ] */

        sim800_parse_str(&ptr, &tmp[0]); //TODO: IP parser
        ptr += 2; /* skip [ ," ] */

        client->port = sim800_parse_int(&ptr);
        ptr += 3; /* skip [ ," ] */

        sim800_parse_str(&ptr, &tmp[0]);

        if (strcmp(tmp, "INITIAL") == STRINGS_MATCH) {
            switch_client_state(client, SIM800_GPRS_CLIENT_STATE_INITIAL);
        }
        else if (strcmp(tmp, "CONNECTING") == STRINGS_MATCH) {
            switch_client_state(client, SIM800_GPRS_CLIENT_STATE_CONNECTING);
        }
        else if (strcmp(tmp, "CONNECTED") == STRINGS_MATCH) {
            switch_client_state(client, SIM800_GPRS_CLIENT_STATE_CONNECTED);
        }
        else if (strcmp(tmp, "REMOTE CLOSING") == STRINGS_MATCH) {
            switch_client_state(client, SIM800_GPRS_CLIENT_STATE_REMOTE_CLOSING);
        }
        else if (strcmp(tmp, "CLOSING") == STRINGS_MATCH) {
            switch_client_state(client, SIM800_GPRS_CLIENT_STATE_CLOSING);
        }
        else if (strcmp(tmp, "CLOSED") == STRINGS_MATCH) {
            switch_client_state(client, SIM800_GPRS_CLIENT_STATE_CLOSED);
        }
        else {
            printf("[CIPSTATUS #%d] Unprocessed: '%s'\n", index, tmp);
            switch_client_state(client, SIM800_GPRS_CLIENT_STATE_UNDEFINED);
        }
    }
}


/**
 *
 */
static void
gprs_ip_state_parser(sim800_t* p, const char* str, void* param)
{
    const char* ptr = str + strlen(GPRS_IP_STATE);

    if (strcmp(ptr, "PDP DEACT") == STRINGS_MATCH) {
        switch_gprs_state(p, SIM800_GPRS_STATE_PDP_DEACT);
    }
    else if (strcmp(ptr, "IP INITIAL") == STRINGS_MATCH) {
        switch_gprs_state(p, SIM800_GPRS_STATE_IP_INITIAL);
    }
    else if (strcmp(ptr, "IP START") == STRINGS_MATCH) {
        switch_gprs_state(p, SIM800_GPRS_STATE_IP_START);
    }
    else if (strcmp(ptr, "IP CONFIG") == STRINGS_MATCH) {
        switch_gprs_state(p, SIM800_GPRS_STATE_IP_CONFIG);
    }
    else if (strcmp(ptr, "IP GPRSACT") == STRINGS_MATCH) {
        switch_gprs_state(p, SIM800_GPRS_STATE_IP_GPRSACT);
    }
    else if (strcmp(ptr, "IP STATUS") == STRINGS_MATCH) {
        switch_gprs_state(p, SIM800_GPRS_STATE_IP_STATUS);
    }
    else if (strcmp(ptr, "IP PROCESSING") == STRINGS_MATCH) {
        switch_gprs_state(p, SIM800_GPRS_STATE_IP_PROCESSING);
    }
    else {
        printf("< %s > Unprocessed: '%s'\n", __func__, ptr);
        switch_gprs_state(p, SIM800_GPRS_STATE_ERROR);
    }
}


/**
 *
 */
static void
conn_state_parser(sim800_t* p, const char* str, void* param)
{
    sim800GprsClient_t* client;

    const char* ptr = str;
    int index = sim800_parse_int(&ptr);

    if (
        index >= GPRS_CLIENTS_INDEX_MIN &&
        index <= GPRS_CLIENTS_INDEX_MAX )
    {
        client = get_client_by_index(p, index);
        ptr += 2; /* skip [ ,_ ] */

        if (strcmp(ptr, "SEND OK") == STRINGS_MATCH) {
            switch_client_tx_stage(client, SIM800_GPRS_CLIENT_TRANSMIT_SUCCESS);
        }
        else if (strcmp(ptr, "SEND FAIL") == STRINGS_MATCH) {
            switch_client_tx_stage(client, SIM800_GPRS_CLIENT_TRANSMIT_FAILED);
        }
        else if (strcmp(ptr, "CONNECT OK") == STRINGS_MATCH) {
            switch_client_state(client, SIM800_GPRS_CLIENT_STATE_CONNECTED);
        }
        else if (strcmp(ptr, "CONNECT FAIL") == STRINGS_MATCH) {
            switch_client_state(client, SIM800_GPRS_CLIENT_STATE_FAILED);
        }
        else if (strcmp(ptr, "CLOSED") == STRINGS_MATCH) {
            switch_client_state(client, SIM800_GPRS_CLIENT_STATE_CLOSED);
        }
        else if (strcmp(ptr, "ALREADY CONNECT") == STRINGS_MATCH) {
            request_cipstatus(p, SIM800_EVENT_COMMAND_RESULT_OK, NULL);
        }
        else {
            printf("< %s > Unprocessed: '%s'\n", __func__, index, ptr);
        }
    }
}


/**
 *
 */
static void
client_transmit_process(sim800_t* p, sim800GprsClient_t* client)
{
    if (client) {
        if (client->Tx.len && client->Tx.Stage == SIM800_GPRS_CLIENT_TRANSMIT_PROCEEDING) {
            while (!circular_buf_full(p->TxCbufHandle) && (client->Tx.idx < client->Tx.len))
            {
                if (client->Tx.ptr) {
                    circular_buf_put(p->TxCbufHandle, client->Tx.ptr[client->Tx.idx++]);
                }
                else {
                    /* send dummy bytes */
                    circular_buf_put(p->TxCbufHandle, 0x00);
                    client->Tx.idx++;
                }
                SIM800_TXE_IT_ENABLE(); /* Transmit data */
            }
            if (client->Tx.idx >= client->Tx.len) {
                _sim800_cmd_unlock(p); /* UNLOCK! */
                switch_client_tx_stage(client, SIM800_GPRS_CLIENT_TRANSMIT_AWAITING_RESULT);
            }
        }
        else {
            _sim800_cmd_unlock(p); /* UNLOCK! */
            switch_client_tx_stage(client, SIM800_GPRS_CLIENT_TRANSMIT_FAILED);
        }
    }
}

/**
 *
 */
static void
prompt_parser(sim800_t* p, const char* str, void* param)
{
    int index = (int)param;
    sim800GprsClient_t* client = get_client_by_index(p, index);

    if (client) {
        switch_client_tx_stage(client, SIM800_GPRS_CLIENT_TRANSMIT_PROCEEDING);
        client_transmit_process(p, client);
    }
    else {
        _sim800_cmd_unlock(p); /* something goes wrong...*/
    }
    sim800_parser_remove(p, SEND_DATA_PROMPT);
}


/**
 *
 */
static void
client_receive_process(sim800_t* p, sim800GprsClient_t* client)
{
    uint8_t byte;

    if (client) {
        if (
            client->Rx.len &&
            client->Rx.Stage == SIM800_GPRS_CLIENT_RECEIVE_PROCEEDING)
        {
            //            while (rb_available(p->pRxRing) && (client->Rx.idx < client->Rx.len)) {
            //                if (client->Rx.ptr) {
            //                    client->Rx.ptr[client->Rx.idx++] = rb_read_byte(p->pRxRing);
            //                }
            //                else {
            //                    /* skip received bytes */
            //                    rb_read_byte(p->pRxRing);
            //                    client->Rx.idx++;
            //                }
            while (!circular_buf_empty(p->RxCbufHandle) && (client->Rx.idx < client->Rx.len))
            {
                circular_buf_get(p->RxCbufHandle, &byte);
                if (client->Rx.ptr) {
                    client->Rx.ptr[client->Rx.idx++] = byte;
                }
                else {
                    /* skip received bytes */
                    client->Rx.idx++;
                }
            }
            if (client->Rx.idx >= client->Rx.len) {
                switch_client_rx_stage(client, SIM800_GPRS_CLIENT_RECEIVE_COMPLETE);
            }
        }
        else {
            switch_client_rx_stage(client, SIM800_GPRS_CLIENT_RECEIVE_ERROR);
        }
    }
}


/**
 * '+RECEIVE,<n>,<data length>:'
 * Received data from remote client (only in multiple connection mode)
 */
static void
receive_parser(sim800_t* p, const char* str, void* param)
{
    sim800GprsClient_t* client;

    const char* ptr = str + strlen(RECEIVE_NOTIFY);
    int index, len;

    index = sim800_parse_int(&ptr);
    len = sim800_parse_int(&ptr);

    for (int i = 0; i < GPRS_CLIENTS_COUNT; i++) {
        client = get_client_by_index(p, i);
        if (client && client->Rx.Stage == SIM800_GPRS_CLIENT_RECEIVE_PROCEEDING)
        {
            /* other connection in receive data process: ERROR */
            switch_client_rx_stage(client, SIM800_GPRS_CLIENT_RECEIVE_ERROR);
        }
    }

    client = get_client_by_index(p, index);

    if (client) {
        client->Rx.len = len;
        client->Rx.idx = 0;

        /* on PENDING_DATA stage callback must provide pointer to buffer or NULL */
        switch_client_rx_stage(client, SIM800_GPRS_CLIENT_RECEIVE_PENDING_DATA);
        switch_client_rx_stage(client, SIM800_GPRS_CLIENT_RECEIVE_PROCEEDING);

        client_receive_process(p, client);
    }
}

/* transmit/receive ---------------------------------------------- */

/**
 *
 */
static void
client_transmit_start(sim800_t* p, int index)
{
    char str[32];
    sim800GprsClient_t* client;

    for (int i = 0; i < GPRS_CLIENTS_COUNT; i++) {
        client = get_client_by_index(p, i);
        if (client && (client->Tx.Stage == SIM800_GPRS_CLIENT_TRANSMIT_PROCEEDING ||
                       client->Tx.Stage == SIM800_GPRS_CLIENT_TRANSMIT_AWAITING_PROMPT ||
                       client->Tx.Stage == SIM800_GPRS_CLIENT_TRANSMIT_AWAITING_RESULT))
        {
            return; /* other connection transmit in progress */
        }
    }

    client = get_client_by_index(p, index);

    if (client && client->Tx.len && client->Tx.Stage == SIM800_GPRS_CLIENT_TRANSMIT_PENDING_DATA)
    {
        snprintf(str, sizeof(str), "AT+CIPSEND=%d,%d\n", index, client->Tx.len);
        if (sim800_cmd(p, str, 1000, NULL, NULL, SIM800_FLOW_ASYNC) == SIM800_RESULT_OK) {
            switch_client_tx_stage(client, SIM800_GPRS_CLIENT_TRANSMIT_AWAITING_PROMPT);
            sim800_parser_add(p, SEND_DATA_PROMPT, prompt_parser, (void*)index);
        }
    }
}



/* state mutations ---------------------------------------------- */

/**
 *
 */
static void
switch_gprs_state(sim800_t* p, sim800GprsState_t NewGprsState)
{
    switch (NewGprsState)
    {
        case SIM800_GPRS_STATE_IP_INITIAL:
            if (p->Gprs.State == SIM800_GPRS_STATE_INITIALIZATION) {
                NewGprsState = SIM800_GPRS_STATE_INITIALIZATION;
            }
            break;

        case SIM800_GPRS_STATE_INITIALIZATION:
            /* wipe all data */
            memset(&p->Gprs, 0x00, sizeof(sim800Gprs_t));

            /* (re)add specific parsers */
            sim800_parser_add(p, C_STATE_RESPONSE,	client_state_parser, 	NULL);
            sim800_parser_add(p, GPRS_IP_STATE, 	gprs_ip_state_parser, 	NULL);
            sim800_parser_add(p, "\e, ",	 	 	conn_state_parser, 		NULL);
            sim800_parser_add(p, RECEIVE_NOTIFY, 	receive_parser, 		NULL);
            sim800_parser_add(p, GPRS_SHUT_OK, 	 	cipshut_parser, 		NULL);
            break;

        case SIM800_GPRS_STATE_PDP_DEACT:
        case SIM800_GPRS_STATE_DISABLED:
            NewGprsState = SIM800_GPRS_STATE_DISABLED;
            for(int i = 0; i < GPRS_CLIENTS_COUNT; i++) {
                switch_client_state(get_client_by_index(p, i),
                                    SIM800_GPRS_CLIENT_STATE_CLOSED);
            }
            break;

        case SIM800_GPRS_STATE_IP_STATUS:
        case SIM800_GPRS_STATE_READY:
            NewGprsState = SIM800_GPRS_STATE_READY;
            for(int i = 0; i < GPRS_CLIENTS_COUNT; i++) {
                switch_client_state(get_client_by_index(p, i),
                                    SIM800_GPRS_CLIENT_STATE_INITIAL);
            }
            break;

        case SIM800_GPRS_STATE_IP_PROCESSING:
            NewGprsState = SIM800_GPRS_STATE_READY;
            break;

        case SIM800_GPRS_STATE_ERROR:
            for (int i = 0; i < GPRS_CLIENTS_COUNT; i++) {
                switch_client_state(get_client_by_index(p, i),
                                    SIM800_GPRS_CLIENT_STATE_UNDEFINED);
            }
            break;

        default:
            break;
    }
    p->Gprs.State = NewGprsState;
    on_gprs_state_callback(p, p->Gprs.State);
}


static bool
is_client_in_transmit_process(sim800GprsClient_t* client)
{
    if (client) {
        return (client->Tx.Stage == SIM800_GPRS_CLIENT_TRANSMIT_PROCEEDING 	||
                client->Tx.Stage == SIM800_GPRS_CLIENT_TRANSMIT_PENDING_DATA  ||
                client->Tx.Stage == SIM800_GPRS_CLIENT_TRANSMIT_AWAITING_PROMPT ||
                client->Tx.Stage == SIM800_GPRS_CLIENT_TRANSMIT_AWAITING_RESULT );
    }
    return false;
}

static bool
is_client_in_receive_process(sim800GprsClient_t* client)
{
    if (client) {
        return (client->Rx.Stage == SIM800_GPRS_CLIENT_RECEIVE_PROCEEDING ||
                client->Rx.Stage == SIM800_GPRS_CLIENT_RECEIVE_PENDING_DATA );
    }
    return false;
}


/**
 *
 */
static void
switch_client_state(sim800GprsClient_t* client,
                    sim800GprsClientState_t NewClientState)
{
    if (client) {
        switch (NewClientState)
        {
            case SIM800_GPRS_CLIENT_STATE_INITIAL:
            case SIM800_GPRS_CLIENT_STATE_CONNECTING:
                memset(client, 0x00, sizeof(sim800GprsClient_t));
                break;

            case SIM800_GPRS_CLIENT_STATE_CLOSED:
            case SIM800_GPRS_CLIENT_STATE_CLOSING:
                if (is_client_in_transmit_process(client)) {
                    switch_client_tx_stage(client, SIM800_GPRS_CLIENT_TRANSMIT_FAILED);
                }
                else {
                    switch_client_tx_stage(client, SIM800_GPRS_CLIENT_TRANSMIT_UNDEFINED);
                }
                if (is_client_in_receive_process(client)) {
                    switch_client_rx_stage(client, SIM800_GPRS_CLIENT_RECEIVE_ERROR);
                }
                else {
                    switch_client_rx_stage(client, SIM800_GPRS_CLIENT_RECEIVE_UNDEFINED);
                }
                break;

            default:
                break;
        }
        client->State = NewClientState;
        on_gprs_client_state_callback(client, client->State);
    }
}


/**
 *
 */
static void
switch_client_tx_stage(sim800GprsClient_t* client,
                       sim800GprsClientTxStage_t NewClientTxStage)
{
    if (client) {
        client->Tx.Stage = NewClientTxStage;
        client->Tx.timestamp = SIM800_GET_TICK();

        if (client->Tx.callback) {
            client->Tx.callback(client, client->Tx.Stage);
        }
        on_gprs_client_transmit_callback(client, client->Tx.Stage);
    }
}


/**
 *
 */
static void
switch_client_rx_stage(sim800GprsClient_t* client,
                       sim800GprsClientRxStage_t NewClientRxStage)
{
    if (client) {
        client->Rx.Stage = NewClientRxStage;
        client->Rx.timestamp = SIM800_GET_TICK();

        if (client->Rx.callback) {
            client->Rx.callback(client, client->Rx.Stage);
        }
        on_gprs_client_receive_callback(client, client->Rx.Stage);
    }
}


/* init sequence ------------------------------------------------- */

/**
 *
 */
static void
gprs_init_process(sim800_t* p, sim800_Event_t ev, void* param)
{
    static int initStage;
    char str[128];

    //    if (p->Gprs.State != SIM800_GPRS_STATE_INITIALIZATION) {
    //    	printf("[GPRS] < %s > Warning: Not in INITIALIZATION\n", __func__);
    //    	return;
    //    }

    switch (ev) {
        case SIM800_EVENT_INITIALIZATION_BEGIN:
            /* start process */
            initStage = 0;
            break;

        case SIM800_EVENT_COMMAND_RESULT_ERROR:
            /* wait, retry... */
            SIM800_DELAY_MS(2000);
            break;

        case SIM800_EVENT_COMMAND_RESULT_OK:
            /* switch to next step */
            initStage++;
            break;

        default:
            /* retry last step */
            break;
    }

    switch (initStage) {
        case 0: /* Deactivate GPRS */
            sim800_cmd(p, DEACTIVATE_GPRS_PDP, 10 * 1000,
                       request_cipstatus, NULL, SIM800_FLOW_ASYNC);
            break;

        case 1: /* Enable multi-ip connections */
            snprintf(str, sizeof(str), "AT+CIPMUX=%d\n", 1);
            sim800_cmd(p, str, 1000, gprs_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

        case 2: /* Attach to GPRS */
            snprintf(str, sizeof(str), "AT+CGATT=%d\n", 1);
            sim800_cmd(p, str, 5 * 1000, request_cipstatus, gprs_init_process, SIM800_FLOW_ASYNC);
            break;

        case 3: /* Set APN[, user, password] */
            snprintf(str, sizeof(str), "AT+CSTT=\"%s\"\n", "internet");
            sim800_cmd(p, str, 10 * 1000, request_cipstatus, gprs_init_process, SIM800_FLOW_ASYNC);
            break;

        case 4: /* Bring Up Wireless Connection */
            sim800_cmd(p, "AT+CIICR\n", 10 * 1000, request_cipstatus, gprs_init_process, SIM800_FLOW_ASYNC);
            break;

        case 5: /* Get Local IP Address. NOTE: NO 'OK\r\n' returned... */
            sim800_parser_add(p, RESPONSE_LOCAL_IP, cifsr_parser, &p->Gprs.IP);
            sim800_cmd(p, "AT+CIFSR\n", 10 * 1000, gprs_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

        default:
            break;
    }
}


/**
 *
 */
static void
gprs_state_machine(sim800_t* p)
{
    static uint32_t ts_every_second 	= 0;
    static uint32_t ts_every_5_seconds 	= 0;
    static uint32_t ts_every_10_seconds = 0;
    static uint32_t ts_every_30_seconds = 0;
    //    static uint32_t ts_every_minute		= 0;
    //    static uint32_t ts_every_10_minutes	= 0;
    //    static uint32_t ts_every_30_minutes	= 0;
    //    static uint32_t ts_every_hour		= 0;

    switch (p->Gprs.State)
    {
        case SIM800_GPRS_STATE_IP_INITIAL:
            switch_gprs_state(p, SIM800_GPRS_STATE_INITIALIZATION);
            gprs_init_process(p, SIM800_EVENT_INITIALIZATION_BEGIN, 0);
            ts_every_5_seconds = SIM800_GET_TICK();
            break;

        case SIM800_GPRS_STATE_IP_CONFIG:
        case SIM800_GPRS_STATE_INITIALIZATION:
            if (!_sim800_is_cmd_locked(p))
            {
                /* every 5 seconds */
                if (SIM800_GET_TICK() - ts_every_5_seconds >= 5 * 1000) {
                	/* Restart initialization process if it hangs */
                	switch_gprs_state(p, SIM800_GPRS_STATE_INITIALIZATION);
                    gprs_init_process(p, SIM800_EVENT_INITIALIZATION_BEGIN, 0);
                    ts_every_5_seconds = SIM800_GET_TICK();
                }
            }
        	break;


        case SIM800_GPRS_STATE_DISABLED:
            // Do nothing...
            break;

        case SIM800_GPRS_STATE_ERROR:
        	printf("< %s > ----------- Restart GPRS! -----------\n", __func__);
        	gprs_enable(p);
        	break;

        case SIM800_GPRS_STATE_READY:
            for (;;) {
                bool io_in_progress = false;
                sim800GprsClient_t* client;

                for (int i = 0; i < GPRS_CLIENTS_COUNT; i++)
                {
                    client = get_client_by_index(p, i);

                    if (client->Tx.Stage == SIM800_GPRS_CLIENT_TRANSMIT_PENDING_DATA) {
                        client_transmit_start(p, i);
                    }
                    if (client->Tx.Stage == SIM800_GPRS_CLIENT_TRANSMIT_PROCEEDING) {
                        client_transmit_process(p, client);
                        io_in_progress = true;
                    }
                    if (client->Rx.Stage == SIM800_GPRS_CLIENT_RECEIVE_PROCEEDING) {
                        client_receive_process(p, client);
                        io_in_progress = true;
                    }

                    /* TX|RX ERROR Finalize */
                    if (client->State == SIM800_GPRS_CLIENT_STATE_CLOSED) {
                        if (is_client_in_transmit_process(client)) {
                            printf("< %s > FINALIZE TX: '%d' bytes\n", __func__, client->Tx.len);
                            switch_client_tx_stage(client, SIM800_GPRS_CLIENT_TRANSMIT_FAILED);
                        }
                        if (is_client_in_receive_process(client)) {
                            printf("< %s > FINALIZE RX: '%d' bytes\n", __func__, client->Rx.len);
                            switch_client_rx_stage(client, SIM800_GPRS_CLIENT_RECEIVE_ERROR);
                        }
                    }
                    if (is_client_in_transmit_process(client) &&
                            ((SIM800_GET_TICK() - client->Tx.timestamp) > 10 * 1000))
                    {
                        printf("< %s > TIMEOUT TX: '%d' bytes\n", __func__, client->Tx.len);
                        switch_client_tx_stage(client, SIM800_GPRS_CLIENT_TRANSMIT_FAILED);
                    }
                    if (is_client_in_receive_process(client) &&
                            ((SIM800_GET_TICK() - client->Rx.timestamp) > 10 * 1000))
                    {
                        printf("< %s > TIMEOUT RX: '%d' bytes\n", __func__, client->Rx.len);
                        switch_client_rx_stage(client, SIM800_GPRS_CLIENT_RECEIVE_ERROR);
                    }
                }
                if (!io_in_progress) {
                    break; /* exit */
                }
                else {
                    SIM800_DELAY_MS(1);
                }
            } /* until exit */

            if (!_sim800_is_cmd_locked(p))
            {
                /* every second */
                if ((SIM800_GET_TICK() - ts_every_second) >= 1 * 1000) {
                    ts_every_second = SIM800_GET_TICK();
                }

                /* every 10 seconds */
                if (SIM800_GET_TICK() - ts_every_10_seconds >= 10 * 1000) {
                    ts_every_10_seconds = SIM800_GET_TICK();
                }

                /* every 30 seconds */
                if (SIM800_GET_TICK() - ts_every_30_seconds >= 30 * 1000) {
                    ts_every_30_seconds = SIM800_GET_TICK();

                    request_cipstatus(p, 0, NULL);
                }

                /* every minute */

                /* every 10 minutes */

                /* every 30 minutes */

                /* every 1 hour */
            }
            break;

        default:
            break;
    }
}


/**
 *
 */
void
sim800_gprs_run(sim800_t* p)
{
    /* Update SIM800 GPRS FSM */
	gprs_state_machine(p);

    /* DO NOT block other threads! */
    SIM800_DELAY_MS(0);
}


/**
 *
 */
bool
gprs_enable(sim800_t* p)
{
    if (p && p->Gprs.State == SIM800_GPRS_STATE_DISABLED) {
        switch_gprs_state(p, SIM800_GPRS_STATE_IP_INITIAL);
        return true; /* SUCCESS */
    }
    return false; /* ERROR */
}


/**
 *
 */
bool
gprs_disable(sim800_t* p)
{
    if (p && p->Gprs.State != SIM800_GPRS_STATE_DISABLED) {
        switch_gprs_state(p, SIM800_GPRS_STATE_DISABLED);
        return true; /* SUCCESS */
    }
    return false; /* ERROR */
}


/* API ----------------------------------------------------------- */

/**
 *
 */
bool
gprs_is_client_connection_ready(sim800_t* p, int index)
{
    sim800GprsClient_t* client = get_client_by_index(p, index);
    return (client) ? (client->State == SIM800_GPRS_CLIENT_STATE_CONNECTED) : false;
}


/**
 *
 */
sim800GprsClient_t*
gprs_client_connect(sim800_t* p, gprsClientParameters_t* parameters)
{
    char str[128];
    sim800GprsClient_t* client;

    if (
        parameters &&
        parameters->mode &&
        parameters->addr &&
        parameters->port &&
        p && p->Gprs.State == SIM800_GPRS_STATE_READY)
    {
        for (int i = 0; i < GPRS_CLIENTS_COUNT; i++)
        {
            client = get_client_by_index(p, i);

            if (
                client->State == SIM800_GPRS_CLIENT_STATE_CLOSED ||
                client->State == SIM800_GPRS_CLIENT_STATE_FAILED ||
                client->State == SIM800_GPRS_CLIENT_STATE_INITIAL)
            {
                snprintf(str, sizeof(str),
                         "AT+CIPSTART=%d,\"%s\",\"%s\",\"%d\"\n", i,
                         parameters->mode, parameters->addr, parameters->port);

                if (sim800_cmd(p, str, 1000 /* timeout in ms */,
                               NULL, NULL, SIM800_FLOW_SYNC) == SIM800_RESULT_OK)
                {
                    switch_client_state(client, SIM800_GPRS_CLIENT_STATE_CONNECTING);
                    client->Tx.callback = parameters->on_transmit;
                    client->Rx.callback = parameters->on_receive;
                    client->ptr 		= parameters->ptr;
                    return client; /* SUCCESS */
                }
            }
        }
    }
    return NULL /* ERROR */;
}


/**
 *
 */
bool
gprs_client_transmit(sim800GprsClient_t* client, uint8_t* data, uint16_t len)
{
    if (
        data && len && client &&
        client->Tx.Stage != SIM800_GPRS_CLIENT_TRANSMIT_PROCEEDING &&
        client->Tx.Stage != SIM800_GPRS_CLIENT_TRANSMIT_AWAITING_PROMPT &&
        client->Tx.Stage != SIM800_GPRS_CLIENT_TRANSMIT_AWAITING_RESULT)
    {
        client->Tx.ptr = data;
        client->Tx.len = len;
        client->Tx.idx = 0;

        switch_client_tx_stage(client, SIM800_GPRS_CLIENT_TRANSMIT_PENDING_DATA);
        return true; /* SUCCESS */
    }
    return false; /* ERROR */
}


/**
 *
 */
int
gprs_get_connection(sim800_t* p,
                    const char* mode,
                    const char* addr,
                    uint16_t port,
                    uint32_t timeout)
{
    char str[128];
    sim800GprsClient_t* client;

    if (mode && addr && port && p && p->Gprs.State == SIM800_GPRS_STATE_READY)
    {
        for (int i = 0; i < GPRS_CLIENTS_COUNT; i++)
        {
            client = get_client_by_index(p, i);

            if (
                client->State == SIM800_GPRS_CLIENT_STATE_CLOSED ||
                client->State == SIM800_GPRS_CLIENT_STATE_FAILED ||
                client->State == SIM800_GPRS_CLIENT_STATE_INITIAL)
            {
                snprintf(str, sizeof(str),
                         "AT+CIPSTART=%d,\"%s\",\"%s\",\"%d\"\n", i, mode, addr, port);

                if (sim800_cmd(p, str, timeout,
                               NULL, NULL, SIM800_FLOW_SYNC) == SIM800_RESULT_OK) {
                    switch_client_state(client, SIM800_GPRS_CLIENT_STATE_CONNECTING);
                    return i; /* SUCCESS: connection index */
                }
            }
        }
    }
    return GPRS_CLIENT_INVALID;
}


/**
 *
 */
bool
gprs_client_send(sim800_t* p, int index,
                 uint8_t* data, uint16_t len)
{
    sim800GprsClient_t* client = get_client_by_index(p, index);

    if (
        data && len && client &&
        client->Tx.Stage != SIM800_GPRS_CLIENT_TRANSMIT_PROCEEDING &&
        client->Tx.Stage != SIM800_GPRS_CLIENT_TRANSMIT_AWAITING_PROMPT)
    {
        client->Tx.ptr = data;
        client->Tx.len = len;
        client->Tx.idx = 0;

        switch_client_tx_stage(client, SIM800_GPRS_CLIENT_TRANSMIT_PENDING_DATA);
        return true;
    }
    return false;
}


/**
 *
 */
bool
gprs_client_recv(sim800_t* p, int index, uint8_t* data, uint16_t len)
{
    sim800GprsClient_t* client = get_client_by_index(p, index);

    if (
        data && len && client &&
        client->Tx.Stage != SIM800_GPRS_CLIENT_TRANSMIT_PROCEEDING)
    {
        client->Rx.ptr = data;
        client->Rx.len = len;
        client->Rx.idx = 0;

        switch_client_rx_stage(client, SIM800_GPRS_CLIENT_RECEIVE_IDLE);
        return true;
    }
    return false;
}


/* callback's ---------------------------------------------------- */

__attribute__((weak)) void
on_gprs_state_callback(sim800_t* p, sim800GprsState_t State) {
    //    printf("[GPRS callback] %s\n", __func__);
}

__attribute__((weak)) void
on_gprs_client_state_callback(sim800GprsClient_t* client,
                              sim800GprsClientState_t State) {
    //    printf("[GPRS callback] %s\n", __func__);
}

__attribute__((weak)) void
on_gprs_client_transmit_callback(sim800GprsClient_t* client,
                                 sim800GprsClientTxStage_t TxStage) {
    //    printf("[GPRS callback] %s\n", __func__);
}

__attribute__((weak)) void
on_gprs_client_receive_callback(sim800GprsClient_t* client,
                                sim800GprsClientRxStage_t RxStage) {
    //    printf("[GPRS callback] %s\n", __func__);
}
