#include "parsers.h"
#include "exported.h"

#include <string.h>
#include <stdbool.h>

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

bool check_comma(const char* text) { return (*text == ','); }

bool parse_action(const char* text, bool* action) {
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

static bool cmd_a_parse(char* ptrPhoneNum, char* ptrTxt) {
    bool action = false;
    bool result = false;
    if (strlen(ptrTxt) == 2 && check_phone_number(ptrPhoneNum)) {
        while (*ptrTxt != '\0') {
            if (*ptrTxt != ',') {
                if (!parse_action(ptrTxt, &action)) {
                    break;
                }
                if (action) {
                    Device.bind(ptrPhoneNum);
                } else {
                    Device.unbind(ptrPhoneNum);
                }
                result = true;
            }
            ptrTxt++;
        }
    }
    return result;
}

static bool cmd_b_parse(char* ptrTxt) {
    bool result = false;
    bool action  = false;
    int stage = 0;
    if (strlen(ptrTxt) == 15) {
        while (*ptrTxt != '\0') {
            if (*ptrTxt != ',') {
                if (stage == 0) {
                    if (!parse_action(ptrTxt, &action)) {
                        break;
                    }
                    stage = 1;
                } else {
                    if (!check_phone_number(ptrTxt)) {
                        break;
                    }
                    if (action) {
                        Device.bind(ptrTxt);
                    } else {
                        Device.unbind(ptrTxt);
                    }
                    result = true;
                }
            }
            ptrTxt++;
        }
    }
    return result;
}

static bool parse_relay_state(const char* text, int* state) {
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

static bool cmd_r_parse(const char* ptrPhoneNum, char* ptrTxt) {
    (void)ptrPhoneNum;
    int relay_1_state = -1;
    int relay_2_state = -1;
    bool result = false;
    int stage         = 0;

    if (strlen(ptrTxt) == 4) {
        while (*ptrTxt != '\0') {
            if (*ptrTxt != ',') {
                if (stage == 0) {
                    if (!parse_relay_state(ptrTxt, &relay_1_state)) {
                        break;
                    }
                    stage = 1;
                } else {
                    if (!parse_relay_state(ptrTxt, &relay_2_state)) {
                        break;
                    }
                    result = true;
                }
            }
            ptrTxt++;
        }
    }
    if (relay_1_state >= 0) {
        discrete_output_set(pRelay_1, (bool)relay_1_state);
        DeviceEvent_t event = (bool)relay_1_state ? DeviceEvent_Relay1On: DeviceEvent_Relay1Off;
        Device.event_append(event);
    }
    if (relay_2_state >= 0) {
        discrete_output_set(pRelay_2, (bool)relay_2_state);
        DeviceEvent_t event = (bool)relay_2_state ? DeviceEvent_Relay2On: DeviceEvent_Relay2Off;
        Device.event_append(event);
    }
    return result;
}

static void cmd_c_parse(char* phone_number, char* text) {
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

            default:
                return;
        }
        stage++;
    }
    char *endPtr;
    uint8_t permissions = (uint8_t)strtol(&notifyPermissions[0], &endPtr, 2);
    if (*endPtr != '\0') {
        return;
    }
    Device.config_set(deviceName, permissions, batteryLevel);
}

void on_new_sms_callback(char* ptrPhoneNum, char* ptrTxt) {
    debug_printf("[Parsers] Start, phone: <%s>, text: <%s>\n", ptrPhoneNum, ptrTxt);
    bool result = false;
    if (*ptrTxt == '$') {
        ptrTxt++;
        if (strlen(ptrTxt) >= 1) {
            switch (*ptrTxt) {
            case 'a':
                result = cmd_a_parse(ptrPhoneNum, ++ptrTxt);
            break;
            case 'b':
                result = cmd_b_parse(++ptrTxt);
            break;
            case 'c':
                cmd_c_parse(ptrPhoneNum, ++ptrTxt);
                result = true;
            break;
            case 'r':
                result = cmd_r_parse(ptrPhoneNum, ++ptrTxt);
            break;
            case 'l':
                if (strlen(ptrTxt) == 1) {
                    debug_printf("[Device] Get numbers list for %s\n", ptrPhoneNum);
                    result = true;
                }
            break;
            case 'g':
                if (strlen(ptrTxt) == 1) {
                    Device.config_get(ptrPhoneNum);
                    result = true;
                }
            break;
            default:
                break;
            }
        }
    }
    if (!result) {
        debug_printf("[Parsers] Error while parsing\n");
    }
}
