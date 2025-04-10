#ifndef EXPORTED_H
#define EXPORTED_H

#include "adc.h"
#include "sim800.h"
#include "device.h"
#include "discrete_input.h"
#include "discrete_output.h"

extern MyAdc_t Adc;

extern Device_t Device;
extern Config_t Config;

extern Sim800Handle_t* Sim800Handle;

extern DiscreteOutput_t* pUserLed;

extern DiscreteOutput_t* pRelay_1;
extern DiscreteOutput_t* pRelay_2;

extern ADC_HandleTypeDef hadc;

#endif //EXPORTED_H
