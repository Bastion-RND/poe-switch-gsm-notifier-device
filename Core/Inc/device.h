#ifndef DEVICE_H
#define DEVICE_H

#include <stdbool.h>

#include "main.h"

#define MAX_PHONE_COUNT 15
#define PHONE_LENGTH    12

typedef enum DeviceState_ {
  DEVICE_STATE_UNDEFINED = 0,
  DEVICE_STATE_IDLE,
} DeviceState_t;

typedef struct Phone_ {
  char number[PHONE_LENGTH + 1];
} Phone_t;

typedef struct Device_ {
  void 	(*init)(void);
  void 	(*run)(void);
  void 	(*bind)(const char*);
  void 	(*unbind)(const char*);
  uint8_t phoneCount;
  Phone_t phoneBook[MAX_PHONE_COUNT];
  bool resetEvent;
  bool tamperEvent;
  DeviceState_t State;
} Device_t;

#pragma pack(push, 1)
typedef struct Config_ {
  uint8_t name[64];
  uint8_t permissions;
  float batteryLowThreshold;
} Config_t;
#pragma pack(pop)

#define EEPROM_ADDR_CONFIG 0
#define EEPROM_ADDR_PHONE_BOOK  (EEPROM_ADDR_CONFIG + sizeof(Config_t))

extern Device_t Device;

void device_create(void);

void device_on_button_reset_pressed_long_callback(void);
void device_on_button_tamper_released_callback(void);

#endif //DEVICE_H
