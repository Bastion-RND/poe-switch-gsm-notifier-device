#ifndef EXPORTED_H
#define EXPORTED_H

#include "sim800.h"
#include "device.h"
#include "discrete_output.h"

extern Device_t Device;

extern Sim800Handle_t* Sim800Handle;

extern DiscreteOutput_t* pRelay_1;
extern DiscreteOutput_t* pRelay_2;

#endif //EXPORTED_H
