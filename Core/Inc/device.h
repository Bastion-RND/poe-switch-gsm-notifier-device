#ifndef DEVICE_H
#define DEVICE_H

#include <stdbool.h>

#include "main.h"

#define MAX_PHONE_COUNT           15
#define PHONE_LENGTH              12
#define MAX_DEVICE_NAME_LENGTH    64
#define NOTIFY_PERMISSION_LENGTH  4

#define SINGLE_SMS_LENGTH_MAX     (256 * 2 - 1)

typedef enum DeviceState_ {
  DEVICE_STATE_UNDEFINED = 0,
  DEVICE_STATE_IDLE,
  DEVICE_STATE_SENDING_SINGLE_SMS,
  DEVICE_STATE_SENDING_MULTIPLY_SMS,
  DEVICE_STATE_AWAIT_RESPONSE,
} DeviceState_t;

typedef enum DeviceEvent_ {
  DeviceEvent_Tamper = 0,
  DeviceEvent_NoPower220,
  DeviceEvent_LowBatt,
  DeviceEvent_Relay1On,
  DeviceEvent_Relay1Off,
  DeviceEvent_Relay2On,
  DeviceEvent_Relay2Off,
} DeviceEvent_t;

#define MAX_EVENTS_COUNT 7

typedef struct Phone_ {
  char number[PHONE_LENGTH + 1];
} Phone_t;

typedef struct DeviceEventHandler_ {
  bool request;
  const char* txt;
  uint32_t timestampMs;
  uint32_t minTimeRepeatMs;
} DeviceEventHandler_t;

typedef struct SmsSender_ {
  char* ptrPhoneNumber;
  char txt[SINGLE_SMS_LENGTH_MAX];
  int eventHandlerIdx;
  int phoneCount;
  int phoneIdx;
} SmsSender_t;

typedef struct Device_ {
  void 	(*init)(void);
  void 	(*run)(void);
  void 	(*bind)(const char*);
  void 	(*unbind)(const char*);
  void  (*config_set)(char*, uint8_t, float);
  void  (*config_get)(char*);
  void  (*append_event)(DeviceEvent_t);
  uint8_t phoneCount;
  Phone_t phoneBook[MAX_PHONE_COUNT];
  DeviceEventHandler_t eventHandlers[MAX_EVENTS_COUNT];
  SmsSender_t smsSender;
  bool resetEvent;
  bool configGetRequest;
  Phone_t requestedPhone;
  DeviceState_t State;
} Device_t;

#pragma pack(push, 1)
typedef struct Config_ {
  char name[MAX_DEVICE_NAME_LENGTH + 1];
  uint8_t permissions;
  float batteryLowThreshold;
} Config_t;
#pragma pack(pop)

#define INIT_TAG  (uint16_t)0xAAAA

#define EEPROM_ADDR_INIT_TAG    0
#define EEPROM_ADDR_CONFIG      EEPROM_ADDR_INIT_TAG + sizeof(INIT_TAG)
#define EEPROM_ADDR_PHONE_BOOK  EEPROM_ADDR_CONFIG + sizeof(Config_t)

extern Device_t Device;

void device_create(void);

void device_on_button_reset_pressed_long_callback(void);
void device_on_button_tamper_released_callback(void);

#endif //DEVICE_H
