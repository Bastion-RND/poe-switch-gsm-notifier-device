#include "device.h"
#include "discrete_input.h"
#include "main.h"

bool discrete_input_ll_init(DiscreteInput_t* p) {
    bool result = false;
    if (p) { /* Initialized in HAL */
        result = true;
    }
    return result;
}

bool discrete_input_ll_get_level(DiscreteInput_t* p) {
    bool b;
    switch (p->id) {
        case BUTTON_220_SENSOR_ID:
            b = HAL_GPIO_ReadPin(ADC_220_GPIO_Port, ADC_220_Pin);
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

uint32_t discrete_input_ll_get_tick() { return HAL_GetTick(); }

void discrete_input_opened_callback(DiscreteInput_t* p) {
    switch (p->id) {
        case BUTTON_TAMPER_ID:
#ifdef DEBUG
            debug_printf("[DI implementation] The door is opened\n");
#endif
            Device.event_append(DeviceEvent_Tamper);
            break;

        default:
            break;
    }
}

void discrete_input_closed_long_callback(DiscreteInput_t* p) {
    switch (p->id) {
        case BUTTON_RESET_ID:
            debug_printf("Button RESET closed long callback\n");
            device_on_button_reset_pressed_long_callback();
            break;

        default:
            break;
    }
}


void discrete_input_closed_extra_long_callback(DiscreteInput_t* p) {
    switch (p->id) {
        case BUTTON_220_SENSOR_ID:
#ifdef DEBUG
            debug_printf("[DI implementation] 220 V is gone\n");
#endif
            Device.event_append(DeviceEvent_NoPower220);
            break;

        default:
            break;
    }
}
