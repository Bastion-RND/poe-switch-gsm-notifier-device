#include <stdbool.h>
#include <string.h>

#include "device.h"

#include <stdio.h>

#include "exported.h"
#include "eeprom_in_flash.h"

static Config_t Config;
Device_t Device;

const char* tamperTxt = "[%s]Дверца шкафа открыта\n%s";
const char* noPower220Txt = "[%s]Пропало напряжение питания 220В\n%s";
const char* lowBatteryTxt = "[%s]Низкий заряд АКБ, %.1fВ\n%s";
const char* relayOnTxt = "[%s]Включено реле %d\n%s";
const char* relayOffTxt = "[%s]Отключено реле %d\n%s";
const char* unsubscribeTxt = "Для отписки отправьте: $a,0";

static void init() {
    uint16_t tag = 0;
    EepromInFlash.read(EEPROM_ADDR_INIT_TAG, (uint8_t*) &tag, sizeof(tag));
    if (tag != INIT_TAG) {
        debug_printf("[Device] default initialization...\n");
        strcpy(Config.name, "Unknown");
        Config.permissions = 0b1111; //TODO magic number
        Config.batteryLowThreshold = 24.0f;  //TODO magic number
        tag = INIT_TAG;
        EepromInFlash.write(EEPROM_ADDR_CONFIG, (uint8_t*) &Config, sizeof(Config_t));
        for (int i =0; i < MAX_PHONE_COUNT; i++) {
            uint16_t eepromAddr = EEPROM_ADDR_PHONE_BOOK + i * sizeof(Phone_t);
            EepromInFlash.write(eepromAddr, (uint8_t*) &Device.phoneBook[i], sizeof(Phone_t));
        }
        EepromInFlash.write(EEPROM_ADDR_INIT_TAG, (uint8_t*) &tag, sizeof(tag));
    }
    EepromInFlash.read(EEPROM_ADDR_CONFIG, (uint8_t*) &Config, sizeof(Config));
    debug_printf("[Device] Name <%s>, ", Config.name);
    debug_printf("permissions <0x%X>, ", Config.permissions);
    debug_printf("battery low TH <%d>\n", (int)Config.batteryLowThreshold);
    for (uint8_t i = 0; i < MAX_PHONE_COUNT; i++) {
        uint16_t eepromAddress = EEPROM_ADDR_PHONE_BOOK + i * sizeof(Phone_t);
        Phone_t phone;
        EepromInFlash.read(eepromAddress, (uint8_t*) &phone, sizeof(Phone_t));
        debug_printf("[Device] Phone book by idx %d: <%s>\n", i, phone.number);
        if (phone.number[0] == '+' && strlen(phone.number) == PHONE_LENGTH) {
            Device.phoneCount++;
            strcpy(Device.phoneBook[i].number, phone.number);
        }
    }
    debug_printf("[Device] Initialized, total numbers count %d\n", Device.phoneCount);
    Device.State = DEVICE_STATE_IDLE;
    for (int i = 0; i < MAX_EVENTS_COUNT; i++) {
        switch ((DeviceEvent_t)i) {
            case DeviceEvent_Tamper:
                Device.eventHandlers[i].txt = tamperTxt;
                Device.eventHandlers[i].minTimeRepeatMs = 10000;
            break;
            case DeviceEvent_NoPower220:
                Device.eventHandlers[i].txt = noPower220Txt;
                Device.eventHandlers[i].minTimeRepeatMs = 10000;
            break;
            case DeviceEvent_LowBatt:
                Device.eventHandlers[i].txt = lowBatteryTxt;
                Device.eventHandlers[i].minTimeRepeatMs = 10000;
            break;
            case DeviceEvent_Relay1On:
                Device.eventHandlers[i].txt = relayOnTxt;
                Device.eventHandlers[i].minTimeRepeatMs = 10000;
            break;
            case DeviceEvent_Relay1Off:
                Device.eventHandlers[i].txt = relayOffTxt;
                Device.eventHandlers[i].minTimeRepeatMs = 10000;
            break;
            case DeviceEvent_Relay2On:
                Device.eventHandlers[i].txt = relayOnTxt;
                Device.eventHandlers[i].minTimeRepeatMs = 10000;
            break;
            case DeviceEvent_Relay2Off:
                Device.eventHandlers[i].txt = relayOffTxt;
                Device.eventHandlers[i].minTimeRepeatMs = 10000;
            break;
        }
    }
}

static void send_sms_callback(Sim800SmsEvent_t ev) {
    if (ev == SIM800_EVENT_SEND_SMS_SUCCESS) {
        Device.configGetRequest = false;
    }
}

static void config_pack(char* ptrTxt, size_t len) {
    char strPermissions[4 + 1];
    for (int i = 0; i < NOTIFY_PERMISSION_LENGTH; i++) {
        int strIdx = 3 - i;
        if (Config.permissions & (1 << i)) {
            strPermissions[strIdx] = '1';
        } else {
            strPermissions[strIdx] = '0';
        }
    }
    strPermissions[NOTIFY_PERMISSION_LENGTH] = '\0';
    char battVoltage[4 + 1];
    snprintf(battVoltage, sizeof(battVoltage), "%.1f", Config.batteryLowThreshold);
    battVoltage[4] = '\0';

    snprintf(ptrTxt, len, "%s,%s,%s\0", Config.name, strPermissions, battVoltage);
    ptrTxt[len - 1] = '\0';
}

void send_multiply_sms_response(Sim800SmsEvent_t ev) {
    (void)ev;
    Device.State = DEVICE_STATE_SENDING_MULTIPLY_SMS;
}

static char* get_phone_num_by_idx(int idx) {
    if (idx < Device.phoneCount) {
        int foundedNumbers = -1;
        for (int i = 0; i < Device.phoneCount; i++) {
            if (Device.phoneBook[i].number[0] == '+') {
                foundedNumbers++;
                if (foundedNumbers == idx) {
                    return Device.phoneBook[i].number;
                }
            }
        }
    }
    return NULL;
}

static void prepare_message(int i) {
    switch ((DeviceEvent_t)i) {
        case DeviceEvent_LowBatt:
            snprintf(
                Device.smsSender.txt,
                SINGLE_SMS_LENGTH_MAX,
                Device.eventHandlers[i].txt,
                Config.name, Adc.getVoltageBattery(), unsubscribeTxt
                );
        break;
        case DeviceEvent_Relay1On:
        case DeviceEvent_Relay1Off:
            snprintf(
                Device.smsSender.txt,
                SINGLE_SMS_LENGTH_MAX,
                Device.eventHandlers[i].txt,
                Config.name, 1, unsubscribeTxt
                );
        break;
        case DeviceEvent_Relay2On:
        case DeviceEvent_Relay2Off:
            snprintf(
                Device.smsSender.txt,
                SINGLE_SMS_LENGTH_MAX,
                Device.eventHandlers[i].txt,
                Config.name, 2, unsubscribeTxt
                );
        break;
        default:
            snprintf(
                Device.smsSender.txt,
                SINGLE_SMS_LENGTH_MAX,
                Device.eventHandlers[i].txt,
                Config.name, unsubscribeTxt
                );
        break;
    }
}

static void run() {
    switch (Device.State) {
        case DEVICE_STATE_IDLE:
            for (int i = 0; i < MAX_EVENTS_COUNT; i++) {
                if (Device.eventHandlers[i].request) {
                    uint32_t elapsed_time_ms = HAL_GetTick() - Device.eventHandlers[i].timestampMs;
                    bool ena_by_time = elapsed_time_ms >= Device.eventHandlers[i].minTimeRepeatMs;
                    if (!ena_by_time) {continue;}
                    prepare_message(i);
                    Device.smsSender.eventHandlerIdx = i;
                    Device.smsSender.phoneCount = Device.phoneCount;
                    Device.smsSender.phoneIdx = 0;
                    Device.State = DEVICE_STATE_SENDING_MULTIPLY_SMS;
                    break;
                }
            }
            break;

        case DEVICE_STATE_SENDING_MULTIPLY_SMS:
            Device.smsSender.ptrPhoneNumber = get_phone_num_by_idx(Device.smsSender.phoneIdx);
            if (Device.smsSender.ptrPhoneNumber == NULL) {
                Device.eventHandlers[Device.smsSender.eventHandlerIdx].request = false;
                Device.eventHandlers[Device.smsSender.eventHandlerIdx].timestampMs = HAL_GetTick();
                Device.State = DEVICE_STATE_IDLE; /* all subscribers have been served */
            }
            else if (sim800_sms_send(Sim800Handle, Device.smsSender.ptrPhoneNumber, Device.smsSender.txt, send_multiply_sms_response)) {
                Device.smsSender.phoneIdx++;
                Device.State = DEVICE_STATE_AWAIT_RESPONSE;
            }
            break;

        default:
            break;

    }

    // if (Device.configGetRequest && !Device.smsMutex) {
    //     if (!sim800_is_locked(Sim800Handle)) {
    //         char msg[100] = {0};
    //         config_pack(msg, sizeof(msg));
    //         sim800_sms_send(Sim800Handle, Device.requestedPhone.number, msg, send_sms_callback);
    //         Device.smsMutex = true;
    //     }
    // }
}

static void bind_phone_number(const char* ptrPhoneNum) {
    if (Device.phoneCount > MAX_PHONE_COUNT - 1) {
        return;
    }
    if (strlen(ptrPhoneNum) != PHONE_LENGTH) {
        return;
    }
    int empty_idx = -1;
    for (uint8_t i = 0; i < MAX_PHONE_COUNT; i++) {
        if (strcmp(&Device.phoneBook[i].number[0], ptrPhoneNum) == 0) {
            debug_printf("[%s] Number <%s> already exist\n", __func__, ptrPhoneNum);
            return;
        }
        if (empty_idx < 0 && Device.phoneBook[i].number[0] == '\0') {
            empty_idx = i;
        }
    }
    if (empty_idx >= 0) {
        uint16_t eepromAddress = EEPROM_ADDR_PHONE_BOOK + empty_idx * sizeof(Phone_t);
        size_t size = EepromInFlash.write(eepromAddress, (uint8_t*)ptrPhoneNum, sizeof(Phone_t));
        if (size == sizeof(Phone_t)) {
            Device.phoneCount++;
            strcpy(Device.phoneBook[empty_idx].number, ptrPhoneNum);
            debug_printf("[%s] A new phone number has been added: <%s> ", __func__, ptrPhoneNum);
            debug_printf("total numbers: %d\n", Device.phoneCount);
        }
    } else {
        debug_printf("[%s] Error, empty index not found\n", __func__);
    }

}

static void unbind_phone_number(const char* phone_number) {
    if (strlen(phone_number) != PHONE_LENGTH) {
        return;
    }
    int exist_idx = -1;
    for (uint8_t i = 0; i < MAX_PHONE_COUNT; i++) {
        Phone_t phone = Device.phoneBook[i];
        if (strcmp(&phone.number[0], phone_number) == 0) {
            exist_idx = i;
            break;
        }
    }
    if (exist_idx >= 0) {
        uint16_t eepromAddress = EEPROM_ADDR_PHONE_BOOK + exist_idx * sizeof(Phone_t);
        memset(Device.phoneBook[exist_idx].number, 0x00, sizeof(Phone_t));
        EepromInFlash.write(eepromAddress, (uint8_t*)Device.phoneBook[exist_idx].number, sizeof(Phone_t));
        Device.phoneCount = (Device.phoneCount > 0) ? Device.phoneCount - 1 : 0;
        debug_printf("[%s] Removed phone number <%s> ", __func__, phone_number);
        debug_printf("total numbers: %d\n", Device.phoneCount);
    } else {
        debug_printf("[%s] Error, phone number <%s> not found\n", __func__, phone_number);
    }
}

static void config_set(char* ptrDeviceName, uint8_t notifyPermissions, float batteryLevel) {
    size_t size = strlen(ptrDeviceName);
    if (size > MAX_DEVICE_NAME_LENGTH) {
        return;
    }
    if (batteryLevel < 1.0f || batteryLevel > 99.9f) {
        return;
    }
    strcpy(Config.name, ptrDeviceName);
    Config.permissions = notifyPermissions;
    Config.batteryLowThreshold = batteryLevel;
    EepromInFlash.write(EEPROM_ADDR_CONFIG, (uint8_t*)&Config, sizeof(Config_t));
}

static void config_get(char* ptrPhoneNum) {
    if (strlen(ptrPhoneNum) == PHONE_LENGTH) {
        strcpy(Device.requestedPhone.number, ptrPhoneNum);
        Device.configGetRequest = true;
    }
}

static void append_event(DeviceEvent_t event) {
    if (event < MAX_EVENTS_COUNT) {
        Device.eventHandlers[(int)event].request = true;
    }
}

void device_on_button_reset_pressed_long_callback(void) {
    if (Device.State != DEVICE_STATE_UNDEFINED) {
        Device.resetEvent = true;
    }
}

void device_create(void) {
    memset(&Device, 0x00, sizeof(Device_t));
    Device.init = init;
    Device.run = run;
    Device.bind = bind_phone_number;
    Device.unbind = unbind_phone_number;
    Device.config_set = config_set;
    Device.config_get = config_get;
    Device.append_event = append_event;
}
