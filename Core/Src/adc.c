#include <string.h>
#include <stdbool.h>

#include "adc.h"
#include "exported.h"
#include "stm32f0xx_hal.h"

uint16_t buf[2 * 32];

MyAdc_t Adc;

void init() {
    HAL_ADC_Start_DMA(&hadc, (uint32_t*)buf, 2 * 32);
}

void HAL_DMA_(ADC_HandleTypeDef* hadc) {
    if(hadc->Instance == ADC1) {
        debug_printf("Complete ADC\n");
    }
}

void adc_create(void) {
    memset(&Adc, 0x00, sizeof(MyAdc_t));
    Adc.init = init;
    Adc.State = MyAdcState_Idle;
}