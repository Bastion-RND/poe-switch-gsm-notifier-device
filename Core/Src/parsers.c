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

static bool extract_field(char** pptrStart, char* dest, size_t destLen) {
    dest[0] = '\0';
    if (*pptrStart == NULL) return false;
    char* ptrEnd = strchr(*pptrStart, ',');
    size_t len = ptrEnd ? (size_t)(ptrEnd - *pptrStart) : strlen(*pptrStart);
    if (len > destLen) return false;
    strncpy(dest, *pptrStart, len);
    dest[len] = '\0';
    *pptrStart = ptrEnd ? ptrEnd + 1 : NULL;
    return true;
}

static bool cmd_r_parse(char* ptrTxt) {
    char strBuf[1 + 1];
    int relay1State = -1;
    int relay2State = -1;
    char* pStart = strchr(ptrTxt, ',');
    if (pStart == NULL) return false;
    pStart += 1;
    if (!extract_field(&pStart, strBuf, 1)) return false;
    if (!parse_relay_state(strBuf, &relay1State)) return false;
    if (!extract_field(&pStart, strBuf, 1)) return false;
    if (!parse_relay_state(strBuf, &relay2State)) return false;
    if (relay1State >= 0) {
        discrete_output_set(pRelay_1, (bool)relay1State);
        // DeviceEvent_t event = (bool)relay1State ? DeviceEvent_Relay1On: DeviceEvent_Relay1Off;
        // Device.event_append(event); //FIXME uncomment it
    }
    if (relay2State >= 0) {
        discrete_output_set(pRelay_2, (bool)relay2State);
        // DeviceEvent_t event = (bool)relay2State ? DeviceEvent_Relay2On: DeviceEvent_Relay2Off;
        // Device.event_append(event);  //FIXME uncomment it
    }
    debug_printf("[Parsers] Relay set state:\n");
    debug_printf("\tRelay 1 - \"%d\"\n", relay1State);
    debug_printf("\tRelay 2 - \"%d\"\n", relay2State);
    return true;
}

static bool cmd_c_parse(const char* ptrPhoneNum, const char* text) {
    char deviceName[MAX_DEVICE_NAME_LENGTH + 1];
    char notifyPermissionsStr[NOTIFY_PERMISSION_LENGTH + 1];
    char batteryLevelStr[10 + 1];

    char* pStart = strchr(text, ',');
    if (pStart == NULL) return false;
    pStart += 1;
    if (!extract_field(&pStart, deviceName, MAX_DEVICE_NAME_LENGTH)) return false;
    if (!extract_field(&pStart, notifyPermissionsStr, NOTIFY_PERMISSION_LENGTH)) return false;
    if (!extract_field(&pStart, batteryLevelStr, 10)) return false;

    if (strlen(notifyPermissionsStr) != NOTIFY_PERMISSION_LENGTH) return false;

    char *endPtr;
    float batteryLevel = strtof(batteryLevelStr, &endPtr);
    if (*endPtr != '\0') return false;

    uint8_t permissions = (uint8_t)strtol(&notifyPermissionsStr[0], &endPtr, 2);
    if (*endPtr != '\0') {
        return false;
    }
    Device.config_set(ptrPhoneNum, deviceName, permissions, batteryLevel);
    return true;
}

void on_new_sms_callback(char* ptrPhoneNum, char* ptrTxt) {
    debug_printf("\n[Parsers] Start, phone: <%s>, text: <%s>\n", ptrPhoneNum, ptrTxt);
    bool result = false;
    if (check_phone_number(ptrPhoneNum) && *ptrTxt == '$') {
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
                result = cmd_c_parse(ptrPhoneNum, ptrTxt);
            break;
            case 'r':
                result = cmd_r_parse(ptrTxt);
            break;
            case 'l':
                if (strlen(ptrTxt) == 1) {
                    Device.phones_list_get(ptrPhoneNum);
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
        debug_printf("[Parsers] ERROR in \"%s\"\n", __func__);
    }
}
