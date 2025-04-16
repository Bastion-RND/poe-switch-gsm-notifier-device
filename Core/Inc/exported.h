#ifndef EXPORTED_H
#define EXPORTED_H

#include "device.h"
#include "discrete_output.h"

extern Device_t Device;
extern Config_t Config;

extern DiscreteOutput_t* pUserLed;

extern DiscreteOutput_t* pRelay_1;
extern DiscreteOutput_t* pRelay_2;

extern ADC_HandleTypeDef hadc;

#endif //EXPORTED_H
