#ifndef ADC_H
#define ADC_H

#include <stdint.h>
#include <stdbool.h>

#define ADC_SAMPLE_COUNT    32
#define ADC_SMOOTH_WINDOW   16
#define ADC_POLL_PERIOD_MS  100

typedef enum MyAdcState_ {
    MyAdcState_Undefined = 0,
    MyAdcState_Idle,
    MyAdcState_WaitingDma,
    MyAdcState_ReadyDma,
} MyAdcState_t;

#pragma pack(push, 1)
typedef struct MovingAverage_ {
    float       value;
    bool        initialized;
    uint32_t    smoothWindow;
} MovingAverage_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct MyAdc_ {
    void            (*init) (void);
    void            (*run) (void);
    float           (*getVoltageBattery) (void);
    float           (*getVoltage220) (void);
    uint32_t        timestampMs;
    MyAdcState_t    State;
    MovingAverage_t MovingAverageBattery;
    MovingAverage_t MovingAverage220;
} MyAdc_t;
#pragma pack(pop)

extern MyAdc_t Adc;

void adc_create(void);

#endif //ADC_H
