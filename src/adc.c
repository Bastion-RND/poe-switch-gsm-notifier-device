#include <math.h>
#include <string.h>

#include "adc.h"
#include "exported.h"
#include "stm32f0xx_hal.h"

uint16_t buf[1 * ADC_SAMPLE_COUNT];

MyAdc_t Adc;

static void
filter_data(const uint16_t* pData, float* pRawBattery) {
    for (int i = 0; i < ADC_SAMPLE_COUNT; i++) {
        *pRawBattery += (float)pData[i];
    }
    *pRawBattery = *pRawBattery/ADC_SAMPLE_COUNT;
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
    Adc.State = MyAdcState_Idle;
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
                    Device.event_append(DeviceEvent_LowBatt);
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
    switch (Adc.State) {
        case MyAdcState_Idle:
            if (HAL_GetTick() - Adc.timestampMs >= ADC_POLL_PERIOD_MS) {
                memset(buf, 0x00, sizeof(buf[0]) * ADC_SAMPLE_COUNT);
                if (HAL_ADC_Start_DMA(&hadc, (uint32_t*)buf, ADC_SAMPLE_COUNT)==HAL_OK) {
                    Adc.timestampMs = HAL_GetTick();
                    Adc.State = MyAdcState_WaitingDma;
                }
            } else {
                check_battery();
            }
        break;

        case MyAdcState_ReadyDma:
            filter_data(&buf[0], &rawBattery);
            rawBattery = calc_voltage(rawBattery, 3.3f);
            moving_average_add(&Adc.MovingAverageBattery, rawBattery, ADC_BATTERY_COEFFICIENT);
            Adc.State = MyAdcState_Idle;
            break;

        default:
            break;
    }
}

static float get_voltage_battery() {
    return Adc.MovingAverageBattery.value;
}

void adc_create(void) {
    memset(&Adc, 0x00, sizeof(MyAdc_t));
    Adc.init = init;
    Adc.run = run;
    Adc.getVoltageBattery = get_voltage_battery;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        HAL_ADC_Stop_DMA(hadc);
        Adc.State = MyAdcState_ReadyDma;
    }
}