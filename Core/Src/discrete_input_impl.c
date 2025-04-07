#include "main.h"
#include "discrete_input.h"
#include "device.h"

#include "sim800.h"
#include "sim800_gsm_types.h"

extern Sim800Handle_t* Sim800Handle;

bool
discrete_input_ll_init(DiscreteInput_t* p) {
    bool result = false;
    if (p) { /* Initialized in HAL */
        result = true;
    }
    return result;
}

bool
discrete_input_ll_get_level(DiscreteInput_t* p) {
    bool b;
    switch (p->id) {
        case BUTTON_SEND_SMS_ID:
            b = HAL_GPIO_ReadPin(SEND_SMS_BTN_GPIO_Port, SEND_SMS_BTN_Pin);
            break;

        case BUTTON_RESET_ID:
            b = HAL_GPIO_ReadPin(RESET_BTN_GPIO_Port, RESET_BTN_Pin);
        break;

        case BUTTON_TAMPER_ID:
            b = HAL_GPIO_ReadPin(TAMPER_BTN_GPIO_Port, TAMPER_BTN_Pin);
        break;

        default:
            b = false;
    }
    return true ? b == GPIO_PIN_SET : false;
}

uint32_t
discrete_input_ll_get_tick() {
    return HAL_GetTick();
}

void send_cb(Sim800SmsEvent_t ev) {
    switch (ev) {
        case SIM800_EVENT_SEND_SMS_ERROR:
            debug_printf("Send ERROR\n");
            break;
        case SIM800_EVENT_SEND_SMS_SUCCESS:
            debug_printf("Send SUCCESS\n");
            break;
        default:
            debug_printf("UNKNOWN EVENT\n");
            break;
    }
}


void
discrete_input_opened_callback(DiscreteInput_t *p) {
    switch (p->id) {
        case BUTTON_TAMPER_ID:
            debug_printf("TAMPER released callback\n");
            device_on_button_tamper_released_callback();
            break;

        default:
            break;
    }
}

void
discrete_input_closed_long_callback(DiscreteInput_t *p) {
    switch (p->id) {
        case BUTTON_RESET_ID:
            debug_printf("Button RESET closed long callback\n");
            device_on_button_reset_pressed_long_callback();
            break;

        default:
            break;
    }
}
