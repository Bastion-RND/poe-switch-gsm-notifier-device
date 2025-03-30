#include <stdio.h>
#include <string.h>

#include "sim800.h"
#include "sim800_const.h"

static uint8_t tx_buf[64];
static uint8_t rx_buf[256];

static Sim800Parser_t ParsersList[SIM800_PARSERS_MAX];

// FIXME
static volatile struct {
    volatile uint32_t fe;
    volatile uint32_t pe;
    volatile uint32_t overrun;
} UartErrorStat;

static void switch_module_state(Sim800Handle_t* p, Sim800ModuleState_t);
static void module_init_process(Sim800Handle_t *p, Sim800Event_t, void* param);

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

    if (strcmp(str + offset, "READY") /* does NOT match */) {
        switch_module_state(p, SIM800_MODULE_STATE_ERROR);
    }
    on_pin_checked_callback(str + offset);
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
            sim800_parser_add(p, RESPONSE_OK, ok_reply_handler, NULL);
            sim800_parser_add(p, RESPONSE_ERROR, error_reply_handler, NULL);
            LL_USART_EnableIT_ERROR(USART2); // FIXME
            SIM800_RXNE_IT_ENABLE();
            HAL_GPIO_WritePin(SIM800_RESET_GPIO_Port, SIM800_RESET_Pin, GPIO_PIN_SET);
            HAL_Delay(1000);
            HAL_GPIO_WritePin(SIM800_RESET_GPIO_Port, SIM800_RESET_Pin, GPIO_PIN_RESET);
        // FIXME: SIM800_POWER_ON();
            break;

        case SIM800_MODULE_STATE_INITIALIZATION:
            debug_printf("[SIM800] switch state to INITIALIZATION\n");
            sim800_parser_add(p, RESPONSE_PIN, cpin_parser, NULL);
            sim800_parser_add(p, REQUEST_MODEL, string_parser, p->Module.model);
            sim800_parser_add(p, REQUEST_SN, string_parser, p->Module.serialNumber);
            sim800_parser_add(p, REQUEST_REV, string_parser, p->Module.revision);
            module_init_process(p, SIM800_EVENT_INITIALIZATION_BEGIN, NULL);
            break;

        case SIM800_MODULE_STATE_ERROR:
            debug_printf("[SIM800] switch state to ERROR\n");
            p->Module.errorTimestamp = SIM800_GET_TICK();
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
    debug_printf("[SIM800] module init process called\n");
    static int stage;
    char str[128];

    switch (event) {
        case SIM800_EVENT_INITIALIZATION_BEGIN:
            stage = 0;
            p->Command.attemptCounter = 0;
            break;

        case SIM800_EVENT_CMD_RESULT_ERR:
            p->Command.attemptCounter++;
            if (p->Command.attemptCounter >= 20) {
                stage = -1;
            } else {
                SIM800_DELAY_MS(1000);
            }
            break;

        case SIM800_EVENT_CMD_RESULT_OK:
            stage++; /* switch to next step */
            p->Command.attemptCounter = 0;
            break;

        default: /* retry last step */
            break;
    }

    switch (stage) {
        case 0:
            debug_printf("[SIM800] init stage 0: check module ready to proceed\n");
            sim800_cmd(p, AT, 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

        case 1:
            debug_printf("[SIM800] init stage 1: reset settings\n");
            sim800_cmd(p, REQUEST_RST_TO_DEF, 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

        case 2:
            debug_printf("[SIM800] init stage 2: get module model\n");
            sim800_cmd(p, REQUEST_MODEL, 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

        case 3:
            debug_printf("[SIM800] init stage 3: get module revision\n");
            sim800_cmd(p, REQUEST_REV, 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

        case 4:
            debug_printf("[SIM800] init stage 4: get module serial number\n");
            sim800_cmd(p, REQUEST_SN, 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

        case 5:
            debug_printf("[SIM800] init stage 5: check SIM card is ready\n");
            sim800_cmd(p, GET_SIM_STATE, 1000, module_init_process, NULL, SIM800_FLOW_ASYNC);
            break;

        case -1:
            switch_module_state(p, SIM800_MODULE_STATE_ERROR);
            break;

        default:
            if (p->Module.State == SIM800_MODULE_STATE_INITIALIZATION) {
                switch_module_state(p, SIM800_MODULE_STATE_READY);
            }
    }
}

Sim800Handle_t *sim800_init(void) {
    Sim800Handle_t* p = malloc(sizeof(Sim800Handle_t));
    memset(p, 0x00, sizeof(Sim800Handle_t));
    p->TxCbufHandle = circular_buf_init(tx_buf, sizeof(tx_buf));
    p->RxCbufHandle = circular_buf_init(rx_buf, sizeof(rx_buf));
    p->ParsersList = ParsersList;
    switch_module_state(p, SIM800_MODULE_STATE_UNDEFINED);
    return p;
}

void sim800_run(Sim800Handle_t* p) {
    static uint32_t ts_every_second = 0;
    static uint32_t ts_every_10_seconds = 0;
    static uint32_t ts_every_30_seconds = 0;

    sim800_rx_ring_parser();

    switch (p->Module.State) {
        case SIM800_MODULE_STATE_UNDEFINED:
            switch_module_state(p, SIM800_MODULE_STATE_INITIALIZATION);
            break;

        case SIM800_MODULE_STATE_INITIALIZATION:
            break;

        case SIM800_MODULE_STATE_ERROR:
            if ((SIM800_GET_TICK() - p->Module.errorTimestamp) >= 10 * 1000) {
                switch_module_state(p, SIM800_MODULE_STATE_UNDEFINED); /* restart */
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
                    if (p->GsmNetwork.State == SIM800_GSM_STATE_READY) {
                        sim800_cmd(p, "AT+CSQ\n", 1000, NULL, NULL, SIM800_FLOW_ASYNC);
                    }
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
        UartErrorStat.fe++;
    }
    if (LL_USART_IsActiveFlag_PE(SIM800_USART)) {
        LL_USART_ClearFlag_PE(SIM800_USART);
        UartErrorStat.pe++;
    }
    if (LL_USART_IsActiveFlag_ORE(SIM800_USART)) {
        LL_USART_ClearFlag_ORE(SIM800_USART);
        UartErrorStat.overrun++;
    }
    if (LL_USART_IsActiveFlag_NE(SIM800_USART)) {
        LL_USART_ClearFlag_NE(SIM800_USART);
        UartErrorStat.fe++;
    }
}

void sim800_restart() {
    SIM800_POWER_OFF();
    SIM800_DELAY_MS(100);
    sim800_init();
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

/**
 *
 */
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
