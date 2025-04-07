#include "main.h"
#include "discrete_input.h"
#include "device.h"

#include "sim800.h"
#include "sim800_gsm_types.h"

extern Sim800Handle_t* Sim800Handle;

bool
discrete_input_ll_init(DiscreteInput_t *p) {
    bool result = false;
    if (p) {
        /* Initialized in HAL */
        result = true;
    }
    return result;
}

bool
discrete_input_get_ll_input(DiscreteInput_t *p) {
    bool b;

    switch (p->id) {
        case BUTTON_SEND_SMS_ID:
            b = HAL_GPIO_ReadPin(BUTTON_SEND_SMS_PORT, BUTTON_SEND_SMS_PIN);
            break;

        case BUTTON_RESET_ID:
            b = HAL_GPIO_ReadPin(BUTTON_RESET_PORT, RESET_BTN_Pin);
        break;

        case BUTTON_TAMPER_ID:
            b = HAL_GPIO_ReadPin(BUTTON_TAMPER_PORT, TAMPER_BTN_Pin);
        break;

        default:
            b = false;
    }
    return (!b == p->Level);
}

uint32_t
discrete_input_get_ll_tick() {
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
button_released_callback(DiscreteInput_t *p) {
    switch (p->id) {
        case BUTTON_TAMPER_ID:
            debug_printf("Button TAMPER released callback\n");
            device_on_button_tamper_released_callback();
            break;

        default:
            break;
    }
}

void
button_pressed_long_callback(DiscreteInput_t *p) {
    switch (p->id) {
        case BUTTON_SEND_SMS_ID:
            debug_printf("Try send SMS\n");
            sim800_sms_send(Sim800Handle, "+79094294096",
                            "The quick brown fox jumps over the lazy dog and runs away quickly", send_cb);

            break;

        case BUTTON_RESET_ID:
            debug_printf("Button RESET pressed long callback\n");
            device_on_button_reset_pressed_long_callback();
            break;

        default:
            break;
    }
}
