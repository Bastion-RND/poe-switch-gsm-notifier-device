#include <stdbool.h>
#include <string.h>

#include "device.h"

#include "eeprom_in_flash.h"

static Config_t Config;
Device_t Device;

static bool check_phone_number(char* phone_number) {
    bool result = false;
    if (strlen(phone_number) == 12 && phone_number[0] == '+') {
        char* phoneNumPtr = &phone_number[1];
        result            = true;
        do {
            if (*phoneNumPtr < '0' || *phoneNumPtr > '9') {
                result = false;
                break;
            }
            phoneNumPtr++;
        } while (*phoneNumPtr != '\0');
    }
    return result;
}

bool check_comma(char* text) { return (*text == ','); }

bool parse_action(char* text, bool* action) {
    bool result = false;
    if (*text == '1') {
        *action = true;
        result  = true;
    } else if (*text == '0') {
        *action = false;
        result  = true;
    }
    return result;
}

static void cmd_a_parse(char* phone_number, char* text) {
    bool action  = false;
    int stage    = 0;
    char* txtPtr = text;
    if (strlen(text) != 2)
        return;
    if (!check_phone_number(phone_number))
        return;
    while (stage < 2) {
        switch (stage) {
            case 0:
                if (!check_comma(txtPtr)) {
                    return;
                }
                break;

            case 1:
                if (!parse_action(txtPtr, &action)) {
                    return;
                }
                if (action) {
                    debug_printf("[Device] Auto bind to %s\n", phone_number);
                } else {
                    debug_printf("[Device] Auto unbind from %s\n", phone_number);
                }
                break;
        }
        txtPtr++;
        stage++;
    }
}

static void cmd_b_parse(char* text) {
    bool action  = false;
    int stage    = 0;
    char* txtPtr = text;
    if (strlen(text) != 15)
        return;
    while (stage < 4) {
        switch (stage) {
            case 0:
            case 2:
                if (!check_comma(txtPtr)) {
                    return;
                }
                break;

            case 1:
                if (!parse_action(txtPtr, &action)) {
                    return;
                }
                break;

            case 3:
                if (check_phone_number(txtPtr)) {
                    if (action) {
                        debug_printf("[Device] Manual bind to %s\n", txtPtr);
                    } else {
                        debug_printf("[Device] Manual unbind from %s\n", txtPtr);
                    }
                }
        }
        txtPtr++;
        stage++;
    }
}

bool parse_relay_state(char* text, int* state) {
    bool result = false;
    if (*text == '1') {
        *state = 1;
        result = true;
    } else if (*text == '0') {
        *state = 0;
        result = true;
    } else if (*text == 'x') {
        *state = -1;
        result = true;
    }
    return result;
}

static void cmd_r_parse(char* phone_number, char* text) {
    int relay_1_state = -1;
    int relay_2_state = -1;

    int stage         = 0;
    char* txtPtr      = text;
    if (strlen(text) != 4)
        return;
    while (stage < 2) {
        if (*txtPtr == ',') {
            txtPtr++;
            continue;
        }
        if (*txtPtr == '\0') {
            break;
        }
        switch (stage) {
            case 0:
                if (!parse_relay_state(txtPtr, &relay_1_state)) {
                    return;
                }
                txtPtr++;
                break;

            case 1:
                if (!parse_relay_state(txtPtr, &relay_2_state)) {
                    return;
                }
                break;
        }
        stage++;
    }
    debug_printf("[Device] Change relay state: RL1: %d, RL2: %d\n", relay_1_state, relay_2_state);
}

static void cmd_c_parse(char* phone_number, char* text) {
    bool action  = false;
    int stage    = 0;
    size_t idx   = 0;
    char* txtPtr = text;
    char deviceName[64 + 1];
    size_t deviceNameLen = sizeof(deviceName) / sizeof(deviceName[0]) - 1;
    char notifyPermissions[4 + 1];
    size_t notifyPermissionsLen = sizeof(notifyPermissions) / sizeof(notifyPermissions[0]) - 1;
    char batteryLevelStr[4 + 1];
    size_t batteryLevelStrLen = sizeof(batteryLevelStr) / sizeof(batteryLevelStr[0]) - 1;
    float batteryLevel        = 0.0f;
    if (!check_phone_number(phone_number))
        return;
    while (stage < 3) {
        if (*txtPtr == ',') {
            txtPtr++;
            continue;
        }
        if (*txtPtr == '\0') {
            break;
        }
        switch (stage) {

            case 0:
                for (idx = 0; idx < deviceNameLen; idx++) {
                    if (!check_comma(txtPtr) && *txtPtr != '\0') {
                        deviceName[idx] = *txtPtr;
                        txtPtr++;
                    } else {
                        break;
                    }
                }
                deviceName[idx] = '\0';
                break;

            case 1:
                for (idx = 0; idx < notifyPermissionsLen; idx++) {
                    if (!check_comma(txtPtr) && *txtPtr != '\0') {
                        notifyPermissions[idx] = *txtPtr;
                        txtPtr++;
                    } else {
                        return;
                    }
                }
                notifyPermissions[idx] = '\0';
                break;

            case 2:
                for (idx = 0; idx < batteryLevelStrLen; idx++) {
                    if (!check_comma(txtPtr) && *txtPtr != '\0') {
                        batteryLevelStr[idx] = *txtPtr;
                        txtPtr++;
                    } else {
                        return;
                    }
                }
                batteryLevelStr[idx] = '\0';
                batteryLevel         = strtof(batteryLevelStr, NULL);
                if (batteryLevel == 0.0 && batteryLevelStr[0] != '0') {
                    return;
                }
                break;
        }
        stage++;
    }
    debug_printf("[Device] New config: device name <%s>, permissions <%s>, "
                 "battery voltage <%f>",
                 deviceName, notifyPermissions, batteryLevel);
}

static void single_char_cmd_parser(char* phone_number, const char c) {
    switch (c) {
        case 'l':
            debug_printf("[Device] Get numbers list for %s\n", phone_number);
            break;
        case 'g':
            debug_printf("[Device] Get config for %s\n", phone_number);
            break;
        default:
            break;
    }
}

void command_parser(char* phone_number, char* text) {
    debug_printf("[Device] command parser start, phone: %s, text: %s\n", phone_number, text);
    char* textPtr = text;
    if (strlen(textPtr) >= 2 && *textPtr == '$') {
        textPtr++;
        if (strlen(textPtr) == 1) {
            single_char_cmd_parser(phone_number, *textPtr);
        } else {
            switch (*textPtr) {
                case 'a':
                    cmd_a_parse(phone_number, ++textPtr);
                    break;
                case 'b':
                    cmd_b_parse(++textPtr);
                    break;
                case 'c':
                    cmd_c_parse(phone_number, ++textPtr);
                    break;
                case 'r':
                    cmd_r_parse(phone_number, ++textPtr);
                    break;
                default:
                    break;
            }
        }
    }
}

void on_new_sms_callback(const char* phone_number, const char* sms_text) { command_parser(phone_number, sms_text); }

static void init() {
    EepromInFlash.read(EEPROM_ADDR_CONFIG, (uint8_t*) &Config, sizeof(Config));
    EepromInFlash.read(EEPROM_ADDR_PHONE_COUNT, &Device.phoneCount, sizeof(Device.phoneCount));
    if (Device.phoneCount > MAX_PHONE_COUNT) {
        Device.phoneCount = 0;
        EepromInFlash.write(EEPROM_ADDR_PHONE_COUNT, (uint8_t*) &Device.phoneCount, sizeof(Device.phoneCount));
    } else if (Device.phoneCount > 0) {
        for (uint8_t i = 0; i < Device.phoneCount; i++) {
            uint16_t eepromAddress = EEPROM_ADDR_PHONE_BOOK + i * sizeof(Device.phoneBook[0]);
            EepromInFlash.read(eepromAddress, (uint8_t*) &Device.phoneBook[i], sizeof(Device.phoneBook[0]));
        }
    }
}

static void run() {
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
    Device.State = DEVICE_STATE_IDLE;
}
