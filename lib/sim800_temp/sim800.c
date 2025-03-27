/*
 * sim800.c
 *
 *  Created on: Sep 21, 2021
 *      Author: sa100
 */

#include <stdio.h>
#include <string.h>

//#include "Debug.h"

#include "sim800.h"

void Error_Handler(void);

//#include "stream_buffer.h"


static uint8_t 				tx_data[64];
//static ringbuffer_t			TxRing_;

static uint8_t 				rx_data[256];
//static ringbuffer_t			RxRing_;

static sim800_Parser_t		Parser[SIM800_PARSERS_MAX];
static sim800_t 			instance;



//FIXME
static volatile struct {
    volatile uint32_t fe;
    volatile uint32_t pe;
    volatile uint32_t overrun;
} UartErrorStat;

/* internal functions -------------------------------------------- */

/* internal function prototypes */
static void switch_module_state(sim800_t*, sim800_ModuleState_t);
static void module_init_process(sim800_t*, sim800_Event_t, void*);
static void module_state_machine(sim800_t*);

/**
 *
 */
bool
_sim800_cmd_lock(sim800_t* p, uint32_t timeout)
{
    uint32_t ts = SIM800_GET_TICK();

    do {
        if (p->Command._mutex == false) {
            p->Command._mutex = true;
            return true; /* SUCCESS */
        }
    }
    while((SIM800_GET_TICK() - ts) <= timeout);
    return false /* ERROR */;
}

/**
 *
 */
void
_sim800_cmd_unlock(sim800_t* p)
{
    p->Command._mutex = false;
}

/**
 *
 */
bool
_sim800_is_cmd_locked(sim800_t* p)
{
    return p->Command._mutex;
}

/**
 *
 */
static sim800_Event_t
send(uint8_t* data, size_t size)
{
    uint32_t ts = SIM800_GET_TICK();
    uint32_t timeout = 1000;

    if (data) {
        while (size) {
            //            if (rb_available_for_write(instance.pTxRing)) {
            //                rb_write_byte(instance.pTxRing, *data);
            if (circular_buf_full(instance.TxCbufHandle) == false) {
                circular_buf_put(instance.TxCbufHandle, *data);
                data++;
                size--;

                SIM800_TXE_IT_ENABLE();
            }
            else {
                SIM800_DELAY_MS(1);
            }
            if ((SIM800_GET_TICK() - ts) >= timeout) {
                return SIM800_EVENT_TRANSMIT_TIMEOUT;
            }
        }
        return SIM800_EVENT_TRANSMIT_SUCCESS;
    }
    return SIM800_EVENT_TRANSMIT_ERROR;
}

/**
 *
 */
static sim800_Event_t
send_string(char* str)
{
    return send((uint8_t*)str, strlen(str));
}


/* default reply handlers */

static void
ok_reply_handler(sim800_t* p, const char* str, void* param)
{
    //printf("Ok!\n");
    _sim800_cmd_unlock(p);

    if (p->Command.callback) {
        p->Command.callback(p, SIM800_EVENT_COMMAND_RESULT_OK,
                            p->Command.callback_param);
    }
}

static void
error_reply_handler(sim800_t* p, const char* str, void* param)
{
    //printf("Error!\n");
    _sim800_cmd_unlock(p);

    if (p->Command.callback) {
        p->Command.callback(p, SIM800_EVENT_COMMAND_RESULT_ERROR,
                            p->Command.callback_param);
    }
}

static void
cpin_parser(sim800_t* p, const char* str, void* param)
{
    const size_t offset = strlen(RESPONSE_PIN);

    if (strcmp(str + offset, "READY") /* does NOT match */) {
        switch_module_state(p, SIM800_STATE_MODULE_ERROR);
    }
    on_pin_checked_callback(p, str + offset);
}

static void
csq_parser(sim800_t* p, const char* str, void* param)
{
    const char* ptr = str + strlen(RESPONSE_RSSI);

    p->Module.RSSI.raw = sim800_parse_int(&ptr);

    if (p->Module.RSSI.raw == 0)
    {
        p->Module.RSSI.dBm = -115;
    } else if (p->Module.RSSI.raw == 1)
    {
        p->Module.RSSI.dBm = -111;
    } else if (p->Module.RSSI.raw >= 2 &&
               p->Module.RSSI.raw <= 31 )
    {
        p->Module.RSSI.dBm = -114 + 2 * p->Module.RSSI.raw;
    }
    else {
        /* not known or not detectable */
        p->Module.RSSI.dBm = 0;
    }
    on_rssi_updated_callback(p, p->Module.RSSI.dBm);
}

/**
 *
 */
static void
string_parser(sim800_t* p, const char* str, void* param)
{
    sim800_readline(p, (char*)param, 128, 1000);
}


/* synchronous command processing */

static sim800_Result_t sync_command_result;

static void
sync_command_callback(sim800_t* p, sim800_Event_t ev, void* param)
{
    sim800_Result_t* pResult = (sim800_Result_t*)param;

    switch (ev)
    {
        case SIM800_EVENT_COMMAND_RESULT_OK:
            *pResult = SIM800_RESULT_OK;
            break;

        case SIM800_EVENT_COMMAND_RESULT_TIMEOUT:
            *pResult = SIM800_RESULT_TIMEOUT;
            break;

        default:
            *pResult = SIM800_RESULT_ERROR;
    }
}




/* --------------------------------------------------------------- */

/**
 *
 */
static void
switch_module_state(sim800_t* p, sim800_ModuleState_t NewState)
{
    switch (NewState)
    {
        case SIM800_STATE_MODULE_UNDEFINED:
            debug_printf("[SIM800] switch state to UNDEFINED\n");
            sim800_parser_add(p, RESPONSE_OK, ok_reply_handler, NULL);
            sim800_parser_add(p, RESPONSE_ERROR, error_reply_handler, NULL);
            LL_USART_EnableIT_ERROR(USART2);//FIXME
            SIM800_RXNE_IT_ENABLE();
            //FIXME: SIM800_POWER_ON();
            break;

        case SIM800_STATE_MODULE_INITIALIZATION:
            debug_printf("[SIM800] switch state to INITIALIZATION\n");
            sim800_parser_add(p, RESPONSE_PIN, cpin_parser, NULL);
            sim800_parser_add(p, RESPONSE_MODEL, string_parser, &p->Module.model);
            sim800_parser_add(p, RESPONSE_SERIAL, string_parser, &p->Module.serialNumber);
            sim800_parser_add(p, RESPONSE_REVISION, string_parser, &p->Module.revision);
            module_init_process(p, SIM800_EVENT_INITIALIZATION_BEGIN, 0);
            break;

        case SIM800_STATE_MODULE_ERROR:
            debug_printf("[SIM800] switch state to ERROR\n");
            break;

        case SIM800_STATE_MODULE_READY:
            debug_printf("[SIM800] switch state to READY\n");
            sim800_parser_remove(p, RESPONSE_PIN);
            sim800_parser_remove(p, RESPONSE_MODEL);
            sim800_parser_remove(p, RESPONSE_SERIAL);
            sim800_parser_remove(p, RESPONSE_REVISION);
            sim800_parser_add(p, RESPONSE_RSSI, csq_parser, NULL);
            break;

        default:
            debug_printf("[SIM800] switch state to UNKNOWN!\n");
            break;
    }
    p->Module.State = NewState;
}

/* init sequence ------------------------------------------------- */

/**
 *
 */
static void
module_init_process(sim800_t* p, sim800_Event_t ev, void* param)
{
    static int stage;
    char str[128];

    switch (ev) {
        case SIM800_EVENT_INITIALIZATION_BEGIN:
            stage = 0; 							/* start process */
            break;

        case SIM800_EVENT_COMMAND_RESULT_ERROR:
            SIM800_DELAY_MS(500); 				/* wait, last step... */
            break;

        case SIM800_EVENT_COMMAND_RESULT_OK:
            stage++; 							/* switch to next step */
            break;

        default:								/* retry last step */
            break;
    }

    switch (stage) {
        case 0: /* Check module ready to proceed */
            debug_printf("[SIM800] stage 0: send AT\n");
            sim800_cmd(p, "AT\n", 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

        case 1: /* Get module MODEL */
            debug_printf("[SIM800] stage 1: send AT+CGMM\n");
        	snprintf(str, sizeof(str), "AT+CGMM\n");
            sim800_cmd(p, str, 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

        case 2: /* Get module REVISION */
            debug_printf("[SIM800] stage 2: send AT+CGMR\n");
        	snprintf(str, sizeof(str), "AT+CGMR\n");
        	sim800_cmd(p, str, 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

        case 3: /* Get module SERIAL NIMBER */
            debug_printf("[SIM800] stage 3: send AT+CGSN\n");
        	snprintf(str, sizeof(str), "AT+CGSN\n");
            sim800_cmd(p, str, 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

        case 4: /* Set Microphone gain */
            debug_printf("[SIM800] stage 4: send AT+CMIC\n");
        	snprintf(str, sizeof(str), "AT+CMIC=%d,%d\n", 0, 4 /* dB */);
            sim800_cmd(p, str, 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

        case 5: /* Disable Microphone bias */
            debug_printf("[SIM800] stage 5: send AT+CMICBIAS\n");
        	snprintf(str, sizeof(str), "AT+CMICBIAS=%d\n", 0);
        	sim800_cmd(p, str, 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

        case 6: /* Check SIM card is ready */
            debug_printf("[SIM800] stage 6: send AT+CPIN\n");
        	snprintf(str, sizeof(str), "AT+CPIN?\n");
            sim800_cmd(p, str, 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

        default:
            if (p->Module.State == SIM800_STATE_MODULE_INITIALIZATION) {
                switch_module_state(p, SIM800_STATE_MODULE_READY);
            }
    }
}


/**
 *
 */
static void
module_state_machine(sim800_t* p)
{
    static uint32_t ts_every_second 	= 0;
    static uint32_t ts_every_10_seconds = 0;
    static uint32_t ts_every_30_seconds = 0;
    static uint32_t ts_every_minute		= 0;
    static uint32_t ts_every_10_minutes	= 0;
    static uint32_t ts_every_30_minutes	= 0;
    static uint32_t ts_every_hour		= 0;


    switch(p->Module.State)
    {
        case SIM800_STATE_MODULE_UNDEFINED:
            printf("[SIM800] State: UNDEFINED\n");
            switch_module_state(p, SIM800_STATE_MODULE_INITIALIZATION);
            break;

        case SIM800_STATE_MODULE_INITIALIZATION:
            break;

        case SIM800_STATE_MODULE_ERROR:
            break;

        case SIM800_STATE_MODULE_READY:
            /* GSM */
            sim800_gsm_run(p);

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

                    /* Check RSSI periodically */
                    if (p->Gsm.State == SIM800_GSM_STATE_READY)
                    {
                        sim800_cmd(p, "AT+CSQ\n", 1000,
                                   NULL, NULL, SIM800_FLOW_ASYNC);
                    }
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


//static uint8_t ucStorageBuffer[ 128 ];
//static StreamBufferHandle_t RxStreamHandle;
//static StaticStreamBuffer_t xStreamBufferStruct;
//static const size_t xTriggerLevel = 1;

/**
 *
 */
sim800_t*
sim800_init(int index)
{
    (void) index;
    memset(&instance, 0x00, sizeof(sim800_t));

    //    /* TX Ring Buffer */
    //    rb_init(&TxRing_, tx_data, sizeof(tx_data));
    //    instance.pTxRing = &TxRing_;

    //    /* RX Ring Buffer */
    //    rb_init(&RxRing_, rx_data, sizeof(rx_data));
    //    instance.pRxRing = &RxRing_;

    instance.TxCbufHandle = circular_buf_init(tx_data, sizeof(tx_data));
    instance.RxCbufHandle = circular_buf_init(rx_data, sizeof(rx_data));


    //    /* FIXME Sb */
    //    if (RxStreamHandle == NULL)
    //        RxStreamHandle = xStreamBufferCreateStatic(
    //                             sizeof(ucStorageBuffer),
    //                             xTriggerLevel,
    //                             ucStorageBuffer,
    //                             &xStreamBufferStruct
    //                         );

    /* Parsers */
    instance.Parser = Parser;

    switch_module_state(&instance, SIM800_STATE_MODULE_UNDEFINED);
    return &instance;
}


/**
 *
 */
void
sim800_run(sim800_t* p)
{
    sim800_rx_ring_parser(p);

    /* Update SIM800 FSM */
    module_state_machine(p);

    /* DO NOT block other threads! */
    SIM800_DELAY_MS(0);
}

static uint8_t 	__buf[1024];
static size_t	__idx;
/**
 *
 */
void
sim800_uart_handler(sim800_t* p)
{
    //    ringbuffer_t* tx = p->pTxRing;
    //    ringbuffer_t* rx = p->pRxRing;
    uint8_t byte;

    if (SIM800_GET_USART_TXE_FLAG()) {
        if (circular_buf_get(instance.TxCbufHandle, &byte) != (-1)) {
            SIM800_TRANSMIT_BYTE(byte);
        }
        else {
            SIM800_TXE_IT_DISABLE();
        }
    }

    if (SIM800_GET_USART_RXNE_FLAG()) {
        byte = SIM800_RECEIVE_BYTE();

        __buf[(__idx++) % sizeof(__buf)] = byte;

        circular_buf_put(instance.RxCbufHandle, byte);

        //        rb_write_byte(rx, byte);
        //        xStreamBufferSendFromISR(RxStreamHandle, &byte, 1, pdFALSE);
    }

    // TODO: USART Errors.
    if (LL_USART_IsActiveFlag_FE(SIM800_USART)) {
        LL_USART_ClearFlag_FE(SIM800_USART);
        UartErrorStat.fe++;
    }
    if (LL_USART_IsActiveFlag_PE(SIM800_USART)) {
        LL_USART_ClearFlag_PE(SIM800_USART);
        UartErrorStat.pe++;
    }
    if (LL_USART_IsActiveFlag_ORE(SIM800_USART)) {
        LL_USART_ClearFlag_ORE(SIM800_USART);
        UartErrorStat.overrun++;
        Error_Handler();
    }
}





/**
 *
 */
void
sim800_restart(sim800_t* p)
{
    SIM800_POWER_OFF();
    SIM800_DELAY_MS(100);

    sim800_init(0 /* FIXME */);
}

/**
 *
 */
bool
sim800_parser_add(sim800_t* p, const char* reply,
                  sim800_parser_handler_t handler, void* param)
{
    if (reply && handler) {
        /* remove if already exists */
        sim800_parser_remove(p, reply);

        for (int i = 0; i < SIM800_PARSERS_MAX; i++) {
            if (p->Parser[i].str == NULL) {
                p->Parser[i].str = reply;
                p->Parser[i].len = strlen(reply);
                p->Parser[i].handler = handler;
                p->Parser[i].handler_param = param;
                return true; /* SUCCESS */
            }
        }
    }
    return false;
}

/**
 *
 */
bool
sim800_parser_remove(sim800_t* p, const char* reply)
{
    if (reply) {
        for (int i = 0; i < SIM800_PARSERS_MAX; i++) {
            if (p->Parser[i].str == reply) {
                p->Parser[i].str = NULL;
                p->Parser[i].len = 0;
                p->Parser[i].handler = NULL;
                p->Parser[i].handler_param = NULL;
                return true; /* SUCCESS */
            }
        }
    }
    return false;
}


sim800_Result_t
sim800_cmd(sim800_t* p,
           const char* cmd,
           uint32_t timeout,
           sim800_callback_t cb,
           void* param,
           sim800_Flow_t flow)
{
    if (_sim800_cmd_lock(p, timeout))
    {
        p->Command.str = (char*)cmd;
        p->Command.timeout = timeout;
        p->Command.ts = SIM800_GET_TICK();

        send_string(p->Command.str);

        if (flow == SIM800_FLOW_ASYNC) {
            p->Command.callback = cb;
            p->Command.callback_param = param;
            return SIM800_RESULT_OK;
        }
        else {
            p->Command.callback = sync_command_callback;
            p->Command.callback_param = &sync_command_result;
        }

        while ((SIM800_GET_TICK() - p->Command.ts) <= p->Command.timeout) {
            if (!_sim800_is_cmd_locked(p)) {
                return sync_command_result;
            }
            SIM800_DELAY_MS(1);
        }
    }
    return SIM800_RESULT_TIMEOUT;
}



/* callback's ---------------------------------------------------- */

__attribute__((weak)) void
on_rssi_updated_callback(sim800_t* p, int dBm)
{
    printf("[SIM800] RSSI: %d dBm\n", dBm);
}

__attribute__((weak)) void
on_pin_checked_callback(sim800_t* p, const char* status)
{
    printf("[SIM800] Pin status: '%s'\n", status);
}
