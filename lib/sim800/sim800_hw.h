#ifndef SIM800_HW_H_
#define SIM800_HW_H_

#include <stdint.h>

typedef enum Sim800HwState_ {
    SIM800_HW_STATE_UNDEFINED = 0,
    SIM800_HW_STATE_REBOOT_PENDING,
    SIM800_HW_STATE_INITIALIZATION,
    SIM800_HW_STATE_ERROR,
} Sim800HwState_t;

typedef struct Sim800Hardware_ {
    Sim800HwState_t State;
    uint32_t timeoutMs;
    uint32_t timestampMs;
    void (*init)(void);
    void (*run)(void);
} Sim800Hardware_t;

void sim800_hw_create();

#endif /* SIM800_HW_H_ */
