#ifndef ADC_H
#define ADC_H

#include <stdint.h>

typedef enum MyAdcState_ {
    MyAdcState_Undefined = 0,
    MyAdcState_Idle,
    MyAdcState_WaitingDma,
} MyAdcState_t;

typedef struct MyAdc_ {
    void            (*init) (void);
    uint32_t        timestampMs;
    uint32_t        pollPeriodMs;
    MyAdcState_t    State;
} MyAdc_t;

extern MyAdc_t Adc;

void adc_create(void);

#endif //ADC_H
