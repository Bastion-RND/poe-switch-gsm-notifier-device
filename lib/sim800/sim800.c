#include <stdio.h>
#include <string.h>

#include "sim800.h"
#include "sim800_const.h"
#include "utf8_xcoder.h"

static uint8_t tx_buf[64];
static uint8_t rx_buf[256];

static Sim800Parser_t ParsersList[SIM800_PARSERS_MAX];

static void switch_module_state(Sim800Handle_t* p, Sim800ModuleState_t);
static void module_init_process(Sim800Handle_t *p, Sim800Event_t, void* param);

void sim800_timer_start(Sim800Handle_t* p, uint32_t period_ms, sim800_callback_t cb) {
    p->Module.Timer.timeout_ms = period_ms;
    p->Module.Timer.callback = cb;
    p->Module.Timer.timestamp = SIM800_GET_TICK();
    p->Module.Timer.active = true;
}

static void sim800_restart(Sim800Handle_t *p, Sim800Event_t event, void* param) {
    (void)param;
    SIM800_POWER_OFF();
    SIM800_DELAY_MS(100);
    switch_module_state(p, SIM800_MODULE_STATE_UNDEFINED);
}

static bool sim800_lock(Sim800Handle_t* p, const uint32_t timeout) {
    uint32_t ts = SIM800_GET_TICK();

    do {
        if (p->Command._mutex == false) {
            p->Command._mutex = true;
            return true;
        }
    } while ((SIM800_GET_TICK() - ts) <= timeout);
    return false;
}

void sim800_unlock(Sim800Handle_t* p) { p->Command._mutex = false; }

bool sim800_is_locked(Sim800Handle_t* p) { return p->Command._mutex; }

static Sim800Event_t send(Sim800Handle_t* p, uint8_t *data, size_t size) {
    uint32_t ts = SIM800_GET_TICK();
    uint32_t timeout = 1000;

    if (data) {
        while (size) {
            if (circular_buf_full(p->TxCbufHandle) == false) {
                circular_buf_put(p->TxCbufHandle, *data);
                data++;
                size--;

                SIM800_TXE_IT_ENABLE();
            } else {
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

static Sim800Event_t send_string(Sim800Handle_t* p, char *str) {
    return send(p, (uint8_t *) str, strlen(str));
}

static void ok_reply_handler(Sim800Handle_t* p, const char *str, void *param) {
    // debug_printf("[SIM800] OK reply handler has been called\n");
    sim800_unlock(p);
    if (p->Command.callback) {
        p->Command.callback(p, SIM800_EVENT_CMD_RESULT_OK, p->Command.callback_param);
    }
}

static void error_reply_handler(Sim800Handle_t* p, const char *str, void *param) {
    // debug_printf("[SIM800] ERROR reply handler has been called\n");
    sim800_unlock(p);
    if (p->Command.callback) {
        p->Command.callback(p, SIM800_EVENT_CMD_RESULT_ERR, p->Command.callback_param);
    }
}

static void cpin_parser(Sim800Handle_t* p, const char *str, void *param) {
    const size_t offset = strlen(RESPONSE_PIN);

    if (strcmp(str + offset, "READY") != 0) {
        switch_module_state(p, SIM800_MODULE_STATE_ERROR);
    }
    on_pin_checked_callback(str + offset);
}

static void creg_parser(Sim800Handle_t *p, const char *str, void *param) {
    const char *ptr = str + strlen("+CREG: ");
    debug_printf("[%s] %s", __func__, str);

    // '+CREG: 1,"66C6","0638"\r\n'
    // TODO: hex parser

    p->Gsm.Network.stat = sim800_parse_int(&ptr);
    ptr += 2; /* skip [ ," ] */

    sim800_parse_str(&ptr, &p->Gsm.Network.lac[0]);
    ptr += 3; /* skip [ "," ] */

    sim800_parse_str(&ptr, &p->Gsm.Network.ci[0]);

    if (p->Gsm.Network.stat == 1) {
        switch_module_state(p, SIM800_MODULE_STATE_READY);
    } else {
        switch_module_state(p, SIM800_MODULE_STATE_NOT_REGISTERED);
    }
}

static void cops_parser(Sim800Handle_t *p, const char *str, void *param) {
    const char *ptr = str + strlen(RESPONSE_GSM_OPERATOR);
    int num;

    // '+COPS: 0,2,"25099"'

    num = sim800_parse_int(&ptr); // <mode>
    ptr += 1;                     /* skip [ , ] */

    num = sim800_parse_int(&ptr); // <format>
    ptr += 2;                     /* skip [ ," ] */

    p->Gsm.Network.mnc = sim800_parse_int(&ptr);
    ptr += 1; /* skip [ , ] */

    sim800_parser_remove(p, RESPONSE_GSM_OPERATOR);
}

static void csq_parser(Sim800Handle_t* p, const char *str, void *param) {
    const char *ptr = str + strlen(RESPONSE_RSSI);

    p->Module.RSSI.raw = sim800_parse_int(&ptr);

    if (p->Module.RSSI.raw == 0) {
        p->Module.RSSI.dBm = -115;
    } else if (p->Module.RSSI.raw == 1) {
        p->Module.RSSI.dBm = -111;
    } else if (p->Module.RSSI.raw >= 2 && p->Module.RSSI.raw <= 31) {
        p->Module.RSSI.dBm = -114 + 2 * p->Module.RSSI.raw;
    } else {
        /* not known or not detectable */
        p->Module.RSSI.dBm = 0;
    }
    on_rssi_updated_callback(p->Module.RSSI.dBm);
}

static void usc2_to_ascii(char* src, char* dest) {
    int counter = (int)strlen(src);
    if (counter % 4 != 0) {
        debug_printf("[%s] error char counter\n", __func__);
    } else {
        while (counter) {
            char c = (str_to_code_point(&src) & 0x7F);
            counter -= 4;
            *dest=c;
            dest++;
        }
    }
    *dest = '\0';
}

char* extract_quoted_part(const char *input, char *output, size_t output_size) {
    output[0] = '\0';
    const char *start = strchr(input, '"');
    if (start == NULL) {
        return NULL;
    }
    char *end = strchr(start + 1, '"');
    if (end == NULL) {
        return NULL;
    }
    size_t length = end - start - 1;
    if (length >= output_size) {
        length = output_size - 1;
    }
    strncpy(output, start + 1, length);
    output[length] = '\0';
    return end + 1;
}

static void cmt_parser(Sim800Handle_t* p, const char* message_header, void *param) {
    (void)param;

    char phone[48 + 1]; /* (12 chars * 4) + \0 */
    extract_quoted_part(message_header, phone, sizeof(phone));
    usc2_to_ascii(phone, p->Gsm.Sms.Recv.phone);

    char message_body[280 + 1]; /* (140 byte * 2 char/byte) + \0 */
    sim800_readline(p, message_body, sizeof(message_body), 1000);
    usc2_to_ascii(message_body, p->Gsm.Sms.Recv.message);

    on_new_sms_callback(p->Gsm.Sms.Recv.phone, p->Gsm.Sms.Recv.message);
}

static void sms_ready_parser(Sim800Handle_t *p, const char *str, void *param) {
    (void)param;
    p->Gsm.Sms.State = SIM800_SMS_STATE_IDLE;
    debug_printf("[%s] %s\n", __func__, str);
}

static void string_parser(Sim800Handle_t* p, const char *str, void *param) {
    sim800_readline(p, (char *) param, 128, 1000);
}

static Sim800Result_t sync_command_result;

static void sync_command_callback(Sim800Handle_t* p, Sim800Event_t event, void *param) {
    Sim800Result_t *pResult = (Sim800Result_t *) param;

    switch (event) {
        case SIM800_EVENT_CMD_RESULT_OK:
            *pResult = SIM800_RESULT_OK;
            break;

        case SIM800_EVENT_CMD_RESULT_TIMEOUT:
            *pResult = SIM800_RESULT_TIMEOUT;
            break;

        default:
            *pResult = SIM800_RESULT_ERROR;
    }
}

static void switch_module_state(Sim800Handle_t* p, Sim800ModuleState_t NewState) {
    switch (NewState) {
        case SIM800_MODULE_STATE_UNDEFINED:
            debug_printf("[SIM800] switch state to UNDEFINED\n");
            SIM800_POWER_ON();
            HAL_Delay(500);
            sim800_parser_add(p, RESPONSE_OK, ok_reply_handler, NULL);
            sim800_parser_add(p, RESPONSE_ERROR, error_reply_handler, NULL);
            LL_USART_EnableIT_ERROR(USART2); // FIXME
            SIM800_RXNE_IT_ENABLE();
            HAL_GPIO_WritePin(SIM800_RESET_GPIO_Port, SIM800_RESET_Pin, GPIO_PIN_SET);
            HAL_Delay(1000);
            HAL_GPIO_WritePin(SIM800_RESET_GPIO_Port, SIM800_RESET_Pin, GPIO_PIN_RESET);
            break;

        case SIM800_MODULE_STATE_INITIALIZATION:
            debug_printf("[SIM800] switch state to INITIALIZATION\n");
            sim800_parser_add(p, RESPONSE_PIN, cpin_parser, NULL);
            sim800_parser_add(p, REQUEST_MODEL, string_parser, p->Module.model);
            sim800_parser_add(p, REQUEST_SN, string_parser, p->Module.serialNumber);
            sim800_parser_add(p, REQUEST_REV, string_parser, p->Module.revision);
            sim800_parser_add(p, "+CREG: ", creg_parser, NULL);
            sim800_parser_add(p, "SMS Ready", sms_ready_parser, NULL);
            sim800_parser_add(p, "+CMT: ", cmt_parser, NULL);
            module_init_process(p, SIM800_EVENT_INITIALIZATION_BEGIN, NULL);
            break;

        case SIM800_MODULE_STATE_ERROR:
            debug_printf("[SIM800] switch state to ERROR\n");
            p->Gsm.Sms.State = SIM800_SMS_STATE_UNDEFINED;
            sim800_timer_start(p, 10000, sim800_restart);
            break;

        case SIM800_MODULE_STATE_NOT_REGISTERED:
            debug_printf("[SIM800] switch state to NOT_REGISTERED\n");
            break;

        case SIM800_MODULE_STATE_READY:
            debug_printf("[SIM800] switch state to READY\n");
            sim800_parser_remove(p, RESPONSE_PIN);
            sim800_parser_remove(p, REQUEST_MODEL);
            sim800_parser_remove(p, REQUEST_SN);
            sim800_parser_remove(p, REQUEST_REV);
            sim800_parser_add(p, RESPONSE_RSSI, csq_parser, NULL);
            break;

        default:
            debug_printf("[SIM800] switch state to UNKNOWN!\n");
            break;
    }
    p->Module.State = NewState;
}

static void module_init_process(Sim800Handle_t* p, Sim800Event_t event, void* param) {
    (void)param;
    static int stage;

    switch (event) {
        case SIM800_EVENT_INITIALIZATION_BEGIN:
            stage = 0;
            p->Command.attemptCounter = 0;
            break;

        case SIM800_EVENT_CMD_RESULT_ERR:
            sim800_timer_start(p, REPEAT_CMD_TIMEOUT_MS, module_init_process);
            break;

        case SIM800_EVENT_TIMER_REACHED:
        case SIM800_EVENT_CMD_RESULT_TIMEOUT:
            p->Command.attemptCounter++;
            if (p->Command.attemptCounter >= CMD_MAX_ATTEMPT) {
                switch_module_state(p, SIM800_MODULE_STATE_ERROR);
            }
            break;

        case SIM800_EVENT_CMD_RESULT_OK:
            stage++; /* switch to next step */
            p->Command.attemptCounter = 0;
            break;

        default:
            return;
    }

    if (p->Module.Timer.active || p->Module.State == SIM800_MODULE_STATE_ERROR) {return;}

    if (p->Module.State == SIM800_MODULE_STATE_INITIALIZATION || p->Module.State == SIM800_MODULE_STATE_UNDEFINED) {
        switch (stage) {
            case 0:
                debug_printf("[SIM800] init stage: check module ready to proceed\n");
            sim800_cmd(p, AT, 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

            case 1:
                debug_printf("[SIM800] init stage: reset settings\n");
            sim800_cmd(p, REQUEST_RST_TO_DEF, 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

            case 2:
            debug_printf("[SIM800] init stage: echo off\n");
            sim800_cmd(p, "ATE0\n", 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

            case 3:
                debug_printf("[SIM800] init stage: get module model\n");
            sim800_cmd(p, REQUEST_MODEL, 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

            case 4:
                debug_printf("[SIM800] init stage: get module revision\n");
            sim800_cmd(p, REQUEST_REV, 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

            case 5:
                debug_printf("[SIM800] init stage: get module serial number\n");
            sim800_cmd(p, REQUEST_SN, 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

            case 6:
                debug_printf("[SIM800] init stage: check SIM card is ready\n");
                sim800_cmd(p, GET_SIM_STATE, 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

            case 7:
                debug_printf("[SIM800] init stage: set network registration info format\n");
                sim800_cmd(p, "AT+CREG=2\n", 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

            case 8:
                debug_printf("[SIM800] init stage: set local timestamp mode\n");
                sim800_cmd(p, "AT+CLTS=1\n", 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

            case 9:
                debug_printf("[SIM800] init stage: set operator selection\n");
                sim800_cmd(p, "AT+COPS=0,2\n", 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

            case 10:
                debug_printf("[SIM800] init stage: set SMS text mode\n");
                sim800_cmd(p, "AT+CMGF=1\n", 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

            case 11:
                debug_printf("[SIM800] init stage: set SMS notification mode\n");
                sim800_cmd(p, "AT+CNMI=1,2,0,0,0\n", 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
                break;

            case 12:
                debug_printf("[SIM800] init stage: set SMS text encoding\n");
                sim800_cmd(p, "AT+CSCS=\"UCS2\"\n", 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

            default:
            break;
        }
    }
}

Sim800Handle_t *sim800_init(void) {
    Sim800Handle_t* p = malloc(sizeof(Sim800Handle_t));
    memset(p, 0x00, sizeof(Sim800Handle_t));
    p->TxCbufHandle = circular_buf_init(tx_buf, sizeof(tx_buf));
    p->RxCbufHandle = circular_buf_init(rx_buf, sizeof(rx_buf));
    p->ParsersList = ParsersList;
    p->Module.Timer.callback = NULL;
    p->Module.Timer.timeout_ms = 0;
    switch_module_state(p, SIM800_MODULE_STATE_UNDEFINED);
    return p;
}

void sim800_run(Sim800Handle_t* p) {
    static uint32_t ts_every_second = 0;
    static uint32_t ts_every_10_seconds = 0;
    static uint32_t ts_every_30_seconds = 0;
    static uint32_t ts_every_minute = 0;

    sim800_rx_ring_parser();

    if (p->Module.Timer.active) {
        uint32_t now = SIM800_GET_TICK();
        if (now - p->Module.Timer.timestamp >= p->Module.Timer.timeout_ms) {
            p->Module.Timer.active = false;
            if (p->Module.Timer.callback) {
                p->Module.Timer.callback(p, SIM800_EVENT_TIMER_REACHED, NULL);
            }
        }
    }

    switch (p->Module.State) {
        case SIM800_MODULE_STATE_UNDEFINED:
            switch_module_state(p, SIM800_MODULE_STATE_INITIALIZATION);
            break;

        case SIM800_MODULE_STATE_INITIALIZATION: /* init event-driven process */
        case SIM800_MODULE_STATE_ERROR: /* event-driven by timer */
            break;

        case SIM800_MODULE_STATE_NOT_REGISTERED:
            /* every minute */
            if ((SIM800_GET_TICK() - ts_every_minute) >= 60 * 1000) {
                ts_every_minute = SIM800_GET_TICK();

                /* Soft reset module */
                sim800_cmd(p, "AT+CFUN=1,1\n", 1000, NULL, NULL, SIM800_FLOW_SYNC);

                /* Reinitialize GSM */
                switch_module_state(p, SIM800_MODULE_STATE_UNDEFINED);
            }
            break;

        case SIM800_MODULE_STATE_READY:
            sim800_gsm_run(p);

            if (!sim800_is_locked(p)) {
                if ((SIM800_GET_TICK() - ts_every_second) >= 1 * 1000) {
                    ts_every_second = SIM800_GET_TICK();
                }

                if (SIM800_GET_TICK() - ts_every_10_seconds >= 10 * 1000) {
                    ts_every_10_seconds = SIM800_GET_TICK();
                }

                if (SIM800_GET_TICK() - ts_every_30_seconds >= 30 * 1000) {
                    ts_every_30_seconds = SIM800_GET_TICK();

                    /* Check RSSI periodically */
                    if (p->Gsm.State == SIM800_GSM_STATE_READY) {
                        sim800_cmd(p, "AT+CSQ\n", 1000, NULL, NULL, SIM800_FLOW_ASYNC);
                    }
                }
            }
            if (p->Gsm.Network.stat && !p->Gsm.Network.mnc) {
                /* Request GSM network operator code (mnc) */
                sim800_parser_add(p, RESPONSE_GSM_OPERATOR, cops_parser, NULL);
                sim800_cmd(p, "AT+COPS?\n", 1000, NULL, NULL, SIM800_FLOW_ASYNC);
            }
            if (SIM800_GET_TICK() - ts_every_second >= 1 * 1000) {
                // if (sim800_cmd(p, "AT+CNMI=1,2,0,0,0\n", 1000, NULL, NULL, SIM800_FLOW_ASYNC) ==
                //     SIM800_RESULT_OK) {
                    ts_every_second = SIM800_GET_TICK();
                    // }
            }
            if (SIM800_GET_TICK() - ts_every_minute >= 60 * 1000) {
                if (sim800_cmd(p, "AT+CMGD=1,4\n", 1000, NULL, NULL, SIM800_FLOW_ASYNC) ==
                    SIM800_RESULT_OK) {
                    debug_printf("\n[GSM] removing all messages in memory\n");
                    ts_every_minute = SIM800_GET_TICK();
                    }
            }
            break;

        default:
            break;
    }
}

static uint8_t __buf[1024];
static size_t __idx;

void sim800_uart_handler(Sim800Handle_t* p) {
    uint8_t byte;

    if (SIM800_GET_USART_TXE_FLAG()) {
        if (circular_buf_get(p->TxCbufHandle, &byte) != (-1)) {
            SIM800_TRANSMIT_BYTE(byte);
        } else {
            SIM800_TXE_IT_DISABLE();
        }
    }
    if (SIM800_GET_USART_RXNE_FLAG()) {
        byte = SIM800_RECEIVE_BYTE();

        __buf[(__idx++) % sizeof(__buf)] = byte;

        circular_buf_put(p->RxCbufHandle, byte);
    }
    if (LL_USART_IsActiveFlag_FE(SIM800_USART)) {
        LL_USART_ClearFlag_FE(SIM800_USART);
    }
    if (LL_USART_IsActiveFlag_PE(SIM800_USART)) {
        LL_USART_ClearFlag_PE(SIM800_USART);
    }
    if (LL_USART_IsActiveFlag_ORE(SIM800_USART)) {
        LL_USART_ClearFlag_ORE(SIM800_USART);
    }
    if (LL_USART_IsActiveFlag_NE(SIM800_USART)) {
        LL_USART_ClearFlag_NE(SIM800_USART);
    }
}

bool sim800_parser_add(Sim800Handle_t* p, const char *reply,
                       sim800_parser_handler_t handler, void *param) {
    if (reply && handler) {
        /* remove if already exists */
        sim800_parser_remove(p, reply);

        for (int i = 0; i < SIM800_PARSERS_MAX; i++) {
            if (p->ParsersList[i].str == NULL) {
                p->ParsersList[i].str = reply;
                p->ParsersList[i].len = strlen(reply);
                p->ParsersList[i].handler = handler;
                p->ParsersList[i].handler_param = param;
                return true;
            }
        }
    }
    return false;
}

bool sim800_parser_remove(Sim800Handle_t* p, const char *reply) {
    if (reply) {
        for (int i = 0; i < SIM800_PARSERS_MAX; i++) {
            if (p->ParsersList[i].str == reply) {
                p->ParsersList[i].str = NULL;
                p->ParsersList[i].len = 0;
                p->ParsersList[i].handler = NULL;
                p->ParsersList[i].handler_param = NULL;
                return true; /* SUCCESS */
            }
        }
    }
    return false;
}

Sim800Result_t sim800_cmd(Sim800Handle_t* p, const char* cmd, uint32_t timeout,
                          sim800_callback_t cb, void *param,
                          sim800_Flow_t flow) {
    Sim800Result_t result = SIM800_RESULT_TIMEOUT;
    if (sim800_lock(p, timeout)) {
        snprintf(p->Command.str, sizeof(p->Command.str), "%s", cmd);
        p->Command.timeout = timeout;
        p->Command.ts = SIM800_GET_TICK();

        send_string(p, p->Command.str);

        if (flow == SIM800_FLOW_ASYNC) {
            p->Command.callback = cb;
            p->Command.callback_param = param;
            result = SIM800_RESULT_OK;
        } else {
            p->Command.callback = sync_command_callback;
            p->Command.callback_param = &sync_command_result;

            while ((SIM800_GET_TICK() - p->Command.ts) <= p->Command.timeout) {
                if (!sim800_is_locked(p)) {
                    result = sync_command_result;
                    break;
                }
                SIM800_DELAY_MS(1);
            }
        }
    }
    return result;
}

__attribute__((weak)) void on_rssi_updated_callback(const int dBm) {
    debug_printf("[SIM800] RSSI callback: %d dBm\n", dBm);
}

__attribute__((weak)) void on_pin_checked_callback(const char *status) {
    debug_printf("[SIM800] on pin checked callback: %s\n", status);
}

__attribute__((weak)) void on_new_sms_callback(
    char *ptrPhoneNum,
    char *ptrTxt
    ) {
    debug_printf(
        "[SIM800] on new SMS callback, number: <%s>, text: <%s>\n",
        ptrPhoneNum,
        ptrTxt
        );
}
