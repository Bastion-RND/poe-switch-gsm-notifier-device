#ifndef DEVICE_H
#define DEVICE_H

#include "main.h"

typedef struct Phone_ {
  uint8_t number[13];
} Phone_t;

typedef struct Device_ {
  void 	(*init)(void);
  uint8_t phoneCount;
  Phone_t phoneBook[50];
} Device_t;

#pragma pack(push, 1)
typedef struct Config_ {
  uint8_t name[64];
  uint8_t permissions;
  float batteryLowThreshold;
} Config_t;
#pragma pack(pop)

#define MAX_PHONE_COUNT 50

#define EEPROM_ADDR_CONFIG 0
#define EEPROM_ADDR_PHONE_COUNT (EEPROM_ADDR_CONFIG + sizeof(Config_t))
#define EEPROM_ADDR_PHONE_BOOK  (EEPROM_ADDR_PHONE_COUNT + sizeof(uint8_t))

extern Device_t Device;

void device_create(void);

#endif //DEVICE_H
