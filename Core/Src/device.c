#include <stdbool.h>
#include <string.h>

#include "device.h"
#include "eeprom_in_flash.h"

static Config_t Config;
Device_t Device;

static void init() {
    EepromInFlash.read(EEPROM_ADDR_CONFIG, (uint8_t*) &Config, sizeof(Config));
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
}

static void run() {
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

void device_on_button_reset_pressed_long_callback(void) {
    if (Device.State != DEVICE_STATE_UNDEFINED) {
        Device.resetEvent = true;
    }
}

void device_on_button_tamper_released_callback(void) {
    if (Device.State != DEVICE_STATE_UNDEFINED) {
        Device.tamperEvent = true;
    }
}

void device_create(void) {
    memset(&Device, 0x00, sizeof(Device_t));
    Device.init = init;
    Device.run = run;
    Device.bind = bind_phone_number;
    Device.unbind = unbind_phone_number;
    Device.State = DEVICE_STATE_IDLE;
}
