#include <math.h>
#include <string.h>

#include "adc.h"
#include "exported.h"
#include "stm32f0xx_hal.h"

uint16_t buf[2 * ADC_SAMPLE_COUNT];

MyAdc_t Adc;

static void
filter_data(uint16_t* pData, float* pRawBattery, float* pRaw220) {
    for (int i = 0; i < (2 * ADC_SAMPLE_COUNT); i++) {
        if (i % 2 == 0) {
            *pRawBattery += (float)pData[i];
        } else {
            *pRaw220 += (float)pData[i];
        }
    }
    *pRawBattery = *pRawBattery/ADC_SAMPLE_COUNT;
    *pRaw220 = *pRaw220/ADC_SAMPLE_COUNT;
}

static float calc_voltage(float raw, float vdda) {
    return raw * vdda / 4095;
}

static void moving_average_add(MovingAverage_t* p, float newValue, float coeff) {
    newValue *= coeff;
    if (p->initialized) {
        p->value -= p->value / (float)ADC_SMOOTH_WINDOW;
        p->value += newValue / (float)ADC_SMOOTH_WINDOW;
    } else {
        p->value = newValue;
        p->initialized = true;
    }
}

static void init() {
    Adc.MovingAverageBattery.initialized = false;
    Adc.MovingAverage220.initialized = false;
    Adc.State = MyAdcState_Idle;
}

static void check_220() {
    if ((HAL_GetTick() - Adc.power220.timestampMs >= ADC_NO_POWER_220_CHECK_PERIOD_MS)) {
        Adc.power220.timestampMs = HAL_GetTick();
        if (!Adc.power220.flag) {
            if (Adc.MovingAverage220.value > 2.0f) { //TODO magic number
                Adc.power220.counter++;
                if (Adc.power220.counter >= ADC_NO_POWER_220_CHECK_MAX_COUNT) {
                    Adc.power220.flag = true;
                    #ifdef ADC_DEBUG
                    debug_printf("[ADC] The 220V supply voltage has been turned off\n");
                    #endif
                    // Device.event_append(DeviceEvent_NoPower220); //FIXME uncomment it
                }
            }
        } else {
            if (Adc.MovingAverage220.value < (2.0f - 0.5f)) { //TODO magic number
                Adc.power220.counter = (Adc.power220.counter > 0) ? Adc.power220.counter - 1 : 0;
                if (Adc.power220.counter == 0) {
                    Adc.power220.flag = false;
                    #ifdef ADC_DEBUG
                    debug_printf("[ADC] The 220V supply voltage has been restored\n");
                    #endif
                }
            }
        }
    }
}

static void check_battery() {
    if ((HAL_GetTick() - Adc.battery.timestampMs >= ADC_BATTERY_CHECK_PERIOD_MS)) {
        Adc.battery.timestampMs = HAL_GetTick();
        if (!Adc.battery.flag) {
            if (Adc.MovingAverageBattery.value < Config.batteryLowThreshold) {
                Adc.battery.counter++;
                if (Adc.battery.counter >= ADC_BATTERY_CHECK_MAX_COUNT) {
                    Adc.battery.flag = true;
                    #ifdef ADC_DEBUG
                    debug_printf("[ADC] The battery has been discharged\n");
                    #endif
                    // Device.event_append(DeviceEvent_LowBatt); //FIXME uncomment it
                }
            }
        } else {
            if (Adc.MovingAverageBattery.value > (Config.batteryLowThreshold + (Config.batteryLowThreshold * 0.05f))) {
                Adc.battery.counter = (Adc.battery.counter > 0) ? Adc.battery.counter - 1 : 0;
                if (Adc.battery.counter == 0) {
                    Adc.battery.flag = false;
                    #ifdef ADC_DEBUG
                    debug_printf("[ADC] The battery has been restored\n");
                    #endif
                }
            }
        }
    }
}

static void run() {
    float rawBattery = 0.0f;
    float raw220 = 0.0f;
    switch (Adc.State) {
        case MyAdcState_Idle:
            if (HAL_GetTick() - Adc.timestampMs >= ADC_POLL_PERIOD_MS) {
                if (HAL_ADC_Start_DMA(&hadc, (uint32_t*)buf, 2 * ADC_SAMPLE_COUNT)==HAL_OK) {
                    Adc.timestampMs = HAL_GetTick();
                    Adc.State = MyAdcState_WaitingDma;
                }
            } else {
                check_220();
                check_battery();
            }
        break;

        case MyAdcState_ReadyDma:
            filter_data(&buf[0], &rawBattery, &raw220);
            rawBattery = calc_voltage(rawBattery, 3.3f);
            raw220 = calc_voltage(raw220, 3.3f);
            moving_average_add(&Adc.MovingAverageBattery, rawBattery, ADC_BATTERY_COEFFICIENT);
            moving_average_add(&Adc.MovingAverage220, raw220, ADC_220_COEFFICIENT);
            Adc.State = MyAdcState_Idle;
            break;

        default:
            break;
    }
}

static float get_voltage_battery() {
    return Adc.MovingAverageBattery.value;
}

static float get_voltage_220() {
    return Adc.MovingAverage220.value;
}

void adc_create(void) {
    memset(&Adc, 0x00, sizeof(MyAdc_t));
    Adc.init = init;
    Adc.run = run;
    Adc.getVoltageBattery = get_voltage_battery;
    Adc.getVoltage220 = get_voltage_220;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        HAL_ADC_Stop_DMA(hadc);
        Adc.State = MyAdcState_ReadyDma;
    }
}