#include <stdbool.h>
#include <string.h>

#include "device.h"

#include <stdio.h>

#include "exported.h"
#include "eeprom_in_flash.h"

Config_t Config;
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
        strcpy(Config.name, "");
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
    for (int i = 0; i < MAX_REQUEST_COUNT; i++) {
        switch ((DeviceRequest_t)i) {
            case DeviceRequest_Config:
            case DeviceRequest_List:
                Device.requestHandlers[i].minTimeRepeatMs = 10000;
            break;
        }
    }
}

void wait_for_multiply_sms_response(Sim800SmsEvent_t ev) {
    (void)ev;
    Device.State = DEVICE_STATE_SENDING_MULTIPLY_SMS;
}

void wait_for_single_sms_response(Sim800SmsEvent_t ev) {
    (void)ev;
    Device.requestHandlers[Device.smsSender.handlerIdx].active = false;
    Device.requestHandlers[Device.smsSender.handlerIdx].timestampMs = HAL_GetTick();
    Device.State = DEVICE_STATE_IDLE;
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

static bool check_permissions(int i) {
    bool result = false;
    switch ((DeviceEvent_t)i) {
        case DeviceEvent_LowBatt:
            if (Config.permissions & (1 << LOW_BATTERY_EVENT_PERMISSION_POS)) result = true;
        break;

        case DeviceEvent_Relay1On:
        case DeviceEvent_Relay1Off:
        case DeviceEvent_Relay2On:
        case DeviceEvent_Relay2Off:
            if (Config.permissions & (1 << RELAY_EVENT_PERMISSION_POS)) result = true;
        break;

        case DeviceEvent_Tamper:
            if (Config.permissions & (1 << TAMPER_EVENT_PERMISSION_POS)) result = true;
        break;

        case DeviceEvent_NoPower220:
            if (Config.permissions & (1 << NO_220_EVENT_PERMISSION_POS)) result = true;
        break;

        default:
            break;
    }
    return result;
}

static bool prepare_event_message(int i) {
    bool result = false;
    switch ((DeviceEvent_t)i) {
        case DeviceEvent_LowBatt:
            snprintf(
                Device.smsSender.txt,
                SINGLE_SMS_LENGTH_MAX,
                Device.eventHandlers[i].txt,
                Config.name, Adc.getVoltageBattery(), unsubscribeTxt
                );
            result = true;
        break;
        case DeviceEvent_Relay1On:
        case DeviceEvent_Relay1Off:
            snprintf(
                Device.smsSender.txt,
                SINGLE_SMS_LENGTH_MAX,
                Device.eventHandlers[i].txt,
                Config.name, 1, unsubscribeTxt
                );
            result = true;
        break;
        case DeviceEvent_Relay2On:
        case DeviceEvent_Relay2Off:
            snprintf(
                Device.smsSender.txt,
                SINGLE_SMS_LENGTH_MAX,
                Device.eventHandlers[i].txt,
                Config.name, 2, unsubscribeTxt
                );
            result = true;
        break;
        case DeviceEvent_Tamper:
        case DeviceEvent_NoPower220:
            snprintf(
                Device.smsSender.txt,
                SINGLE_SMS_LENGTH_MAX,
                Device.eventHandlers[i].txt,
                Config.name, unsubscribeTxt
                );
            result = true;
        break;
        default:
        break;
    }
    return result;
}

bool join_numbers() {
    bool result = false;
    Device.smsSender.txt[0] = '\0';
    for (int i = 0; i < Device.phoneCount; i++) {
        char* ptrPhone = get_phone_num_by_idx(i);
        if (ptrPhone == NULL) {
            break;
        }
        size_t remainingLen = SINGLE_SMS_LENGTH_MAX - strlen(Device.smsSender.txt) - 1;
        if (i > 0) {
            strncat(Device.smsSender.txt, " ", remainingLen);
        }
        strncat(Device.smsSender.txt, ptrPhone, remainingLen);
    }
    if (strlen(Device.smsSender.txt) > 0) result = true;
    return result;
}

static bool prepare_response_message(int request) {
    bool result = false;
    int idx = 0;
    switch ((DeviceRequest_t)request) {
        case DeviceRequest_Config:
            char strPermissions[4 + 1];
            for (idx = 0; idx < NOTIFY_PERMISSION_LENGTH; idx++) {
                if (Config.permissions & (1 << idx)) {
                    strPermissions[3 - idx] = '1';
                } else {
                    strPermissions[3 - idx] = '0';
                }
            }
            strPermissions[NOTIFY_PERMISSION_LENGTH] = '\0';
            char battVoltage[4 + 1];
            snprintf(battVoltage, sizeof(battVoltage), "%.1f", Config.batteryLowThreshold);
            battVoltage[4] = '\0';
            snprintf(Device.smsSender.txt, SINGLE_SMS_LENGTH_MAX, "%s,%s,%s", Config.name, strPermissions, battVoltage);
            result = true;
            break;
        case DeviceRequest_List:
            result = join_numbers();
        default:
            break;
    }
    return result;
}

static void run() {
    switch (Device.State) {
        case DEVICE_STATE_IDLE:
            for (int i = 0; i < MAX_EVENTS_COUNT; i++) {
                if (Device.eventHandlers[i].active) {
                    uint32_t elapsed_time_ms = HAL_GetTick() - Device.eventHandlers[i].timestampMs;
                    bool ena_by_time = elapsed_time_ms >= Device.eventHandlers[i].minTimeRepeatMs;
                    if (!ena_by_time) {continue;}
                    if (!check_permissions(i)) {
                        Device.eventHandlers[i].active = false;
                        continue;
                    }
                    prepare_event_message(i);
                    if (!prepare_event_message(i)) {
                        Device.eventHandlers[i].active = false;
                        continue;
                    }
                    Device.smsSender.handlerIdx = i;
                    Device.smsSender.phoneCount = Device.phoneCount;
                    Device.smsSender.phoneIdx = 0;
                    Device.State = DEVICE_STATE_SENDING_MULTIPLY_SMS;
                    return;
                }
            }
            for (int i = 0; i < MAX_REQUEST_COUNT; i++) {
                if (Device.requestHandlers[i].active) {
                    uint32_t elapsed_time_ms = HAL_GetTick() - Device.requestHandlers[i].timestampMs;
                    bool ena_by_time = elapsed_time_ms >= Device.requestHandlers[i].minTimeRepeatMs;
                    if (!ena_by_time) {continue;}
                    if (!prepare_response_message(i)) {
                        Device.requestHandlers[i].active = false;
                        continue;
                    }
                    Device.smsSender.handlerIdx = i;
                    Device.smsSender.ptrPhoneNumber = Device.requestHandlers[i].phone.number;
                    Device.State = DEVICE_STATE_SENDING_SINGLE_SMS;
                    return;
                }
            }
        break;

        case DEVICE_STATE_SENDING_MULTIPLY_SMS:
            Device.smsSender.ptrPhoneNumber = get_phone_num_by_idx(Device.smsSender.phoneIdx);
            if (Device.smsSender.ptrPhoneNumber == NULL) {
                Device.eventHandlers[Device.smsSender.handlerIdx].active = false;
                Device.eventHandlers[Device.smsSender.handlerIdx].timestampMs = HAL_GetTick();
                Device.State = DEVICE_STATE_IDLE; /* all subscribers have been served */
            }
            else if (sim800_sms_send(Sim800Handle, Device.smsSender.ptrPhoneNumber, Device.smsSender.txt, wait_for_multiply_sms_response)) {
                Device.smsSender.phoneIdx++;
                Device.State = DEVICE_STATE_AWAIT_RESPONSE;
            }
        break;

        case DEVICE_STATE_SENDING_SINGLE_SMS:
            if (sim800_sms_send(Sim800Handle, Device.smsSender.ptrPhoneNumber, Device.smsSender.txt, wait_for_single_sms_response)) {
                Device.State = DEVICE_STATE_AWAIT_RESPONSE;
            }
        break;

        case DEVICE_STATE_RESET:
            for (int i = 0; i < MAX_EVENTS_COUNT; i++) {
                Device.eventHandlers[i].active = false;
            }
            for (int i = 0; i < MAX_REQUEST_COUNT; i++) {
                Device.requestHandlers[i].active = false;
            }
            for (int i = 0; i < MAX_PHONE_COUNT; i++) {
                memset(Device.phoneBook[i].number, 0x00, sizeof(Phone_t));
            }
            Device.phoneCount = 0;
            memset(&Config, 0x00, sizeof(Config_t));
            Config.permissions = 0b1111; //TODO magic number
            Config.batteryLowThreshold = 24.0f;  //TODO magic number
            EepromInFlash.write(EEPROM_ADDR_CONFIG, (uint8_t*)&Config, sizeof(Config_t));
            for (int i = 0; i < MAX_PHONE_COUNT; i++) {
                uint16_t eepromAddr = EEPROM_ADDR_PHONE_BOOK + i * sizeof(Phone_t);
                EepromInFlash.write(eepromAddr, (uint8_t*) &Device.phoneBook[i], sizeof(Phone_t));
            }
            discrete_output_reset(pUserLed);
            discrete_output_meander_start(pUserLed, 100, 100, 10);
            Device.userLed.State = UserLedState_ResetDevice;
            Device.State = DEVICE_STATE_IDLE;

        default:
            break;
    }

    switch (Device.userLed.State) {
        case UserLedState_Undefined:
            if (Sim800Handle->Gsm.State == SIM800_GSM_STATE_READY) {
                discrete_output_set(pUserLed,true);
                Device.userLed.State = UserLedState_GsmReady;
            } else {
                discrete_output_reset(pUserLed);
                discrete_output_meander_start(pUserLed, 1000, 1000, DISCRETE_OUTPUT_COUNT_INFINITE);
                Device.userLed.State = UserLedState_GsmNotReady;
            }
        break;

        case UserLedState_GsmNotReady:
            if (Sim800Handle->Gsm.State == SIM800_GSM_STATE_READY) {
                discrete_output_set(pUserLed,true);
                Device.userLed.State = UserLedState_GsmReady;
            }
        break;

        case UserLedState_GsmReady:
            if (Sim800Handle->Gsm.State != SIM800_GSM_STATE_READY) {
                discrete_output_reset(pUserLed);
                discrete_output_meander_start(pUserLed, 1000, 1000, DISCRETE_OUTPUT_COUNT_INFINITE);
                Device.userLed.State = UserLedState_GsmNotReady;
            }
        break;

        case UserLedState_ResetDevice:
            if (!discrete_output_is_in_sequence(pUserLed)) {
                HAL_NVIC_SystemReset();
                Device.userLed.State = UserLedState_Undefined;
            }
        break;
    }
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

static void config_set(const char* ptrPhoneNum, char* ptrDeviceName, uint8_t notifyPermissions, float batteryLevel) {
    if (strlen(ptrDeviceName) > MAX_DEVICE_NAME_LENGTH) {
        debug_printf("\n[Device] ERROR in \"%s\"\n", __func__);
        debug_printf("\tName \"%s\" is too long\n", ptrDeviceName);
        return;
    }
    if (batteryLevel < 1.0f || batteryLevel > 99.9f) {
        debug_printf("\n[Device] ERROR in \"%s\"\n", __func__);
        debug_printf("\tBattery level \"%d mV\" is out of range\n", (int)(batteryLevel * 1000));
        return;
    }
    bool authorized = false;
    for (int i = 0; i < MAX_PHONE_COUNT; i++) {
        if (strcmp(ptrPhoneNum, Device.phoneBook[i].number) == 0) {
            authorized = true;
            break;
        }
    }
    if (!authorized) {
        debug_printf("\n[Device] ERROR in \"%s\"\n", __func__);
        debug_printf("\tUnauthorized phone number \"%s\" to change config\n", ptrPhoneNum);
        return;
    }
    strcpy(Config.name, ptrDeviceName);
    Config.permissions = notifyPermissions;
    Config.batteryLowThreshold = batteryLevel;
    EepromInFlash.write(EEPROM_ADDR_CONFIG, (uint8_t*)&Config, sizeof(Config_t));
    debug_printf("\n[Device] New config are saved:\n\tName \"%s\", ", ptrDeviceName);
    debug_printf("permissions \"0x%X\", ", Config.permissions);
    debug_printf("battery low level \"%d mV\"\n", (int)(Config.batteryLowThreshold * 1000));
}

static void config_get(char* ptrPhoneNum) {
    if (strlen(ptrPhoneNum) == PHONE_LENGTH) {
        if (!Device.requestHandlers[DeviceRequest_Config].active) {
            strcpy(Device.requestHandlers[DeviceRequest_Config].phone.number, ptrPhoneNum);
            Device.requestHandlers[DeviceRequest_Config].active = true;
        }
    }
}

static void phones_list_get(char* ptrPhoneNum) {
    if (strlen(ptrPhoneNum) == PHONE_LENGTH) {
        if (!Device.requestHandlers[DeviceRequest_List].active) {
            strcpy(Device.requestHandlers[DeviceRequest_List].phone.number, ptrPhoneNum);
            Device.requestHandlers[DeviceRequest_List].active = true;
        }
    }
}

static void event_append(DeviceEvent_t event) {
    if (event < MAX_EVENTS_COUNT) {
        Device.eventHandlers[(int)event].active = true;
    }
}

void device_on_button_reset_pressed_long_callback(void) {
    if (Device.State != DEVICE_STATE_UNDEFINED) {
        Device.State = DEVICE_STATE_RESET;
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
    Device.phones_list_get = phones_list_get;
    Device.event_append = event_append;
}
