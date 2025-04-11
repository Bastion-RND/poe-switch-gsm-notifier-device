#ifndef DEVICE_H
#define DEVICE_H

#include <stdbool.h>

#include "main.h"
#include "sim800_gsm.h"

#define MAX_PHONE_COUNT           15
#define PHONE_LENGTH              12
#define MAX_DEVICE_NAME_LENGTH    (size_t)(64 * 2)
#define NOTIFY_PERMISSION_LENGTH  4

#define NO_220_EVENT_PERMISSION_POS       0
#define RELAY_EVENT_PERMISSION_POS        1
#define LOW_BATTERY_EVENT_PERMISSION_POS  2
#define TAMPER_EVENT_PERMISSION_POS       3

typedef enum DeviceState_ {
  DEVICE_STATE_UNDEFINED = 0,
  DEVICE_STATE_IDLE,
  DEVICE_STATE_SENDING_SINGLE_SMS,
  DEVICE_STATE_SENDING_MULTIPLY_SMS,
  DEVICE_STATE_AWAIT_RESPONSE,
  DEVICE_STATE_RESET,
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

typedef enum DeviceRequest_ {
  DeviceRequest_Config = 0,
  DeviceRequest_List,
} DeviceRequest_t;

typedef enum UserLedState_ {
  UserLedState_Undefined = 0,
  UserLedState_GsmNotReady,
  UserLedState_GsmReady,
  UserLedState_ResetDevice,
} UserLedState_t;

#define MAX_EVENTS_COUNT  7
#define MAX_REQUEST_COUNT 2

typedef struct Phone_ {
  char number[PHONE_LENGTH + 1];
} Phone_t;

typedef struct DeviceEventHandler_ {
  bool active;
  const char* txt;
  uint32_t timestampMs;
  uint32_t minTimeRepeatMs;
} DeviceEventHandler_t;

typedef struct DeviceRequestHandler_ {
  bool active;
  Phone_t phone;
  uint32_t timestampMs;
  uint32_t minTimeRepeatMs;
} DeviceRequestHandler_t;

typedef struct SmsSender_ {
  char* ptrPhoneNumber;
  char txt[SINGLE_SMS_LENGTH_MAX + 1];
  int handlerIdx;
  int phoneCount;
  int phoneIdx;
} SmsSender_t;

typedef struct UserLed_ {
  UserLedState_t State;
} UserLed_t;

typedef struct Device_ {
  void 	(*init)(void);
  void 	(*run)(void);
  void 	(*bind)(const char*);
  void 	(*unbind)(const char*);
  void  (*config_set)(const char*, char*, uint8_t, float);
  void  (*config_get)(char*);
  void  (*phones_list_get)(char*);
  void  (*event_append)(DeviceEvent_t);
  uint8_t phoneCount;
  Phone_t phoneBook[MAX_PHONE_COUNT];
  DeviceEventHandler_t eventHandlers[MAX_EVENTS_COUNT];
  DeviceRequestHandler_t requestHandlers[MAX_REQUEST_COUNT];
  SmsSender_t smsSender;
  bool resetEvent;
  DeviceState_t State;
  UserLed_t userLed;
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
