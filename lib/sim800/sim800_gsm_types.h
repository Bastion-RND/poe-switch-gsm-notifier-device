/*
 * types.h
 *
 *  Created on: Sep 27, 2021
 *      Author: sa100
 */

#ifndef SIM800_GSM_TYPES_H_
#define SIM800_GSM_TYPES_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum sim800GsmState_ {
  SIM800_GSM_STATE_UNDEFINED = 0,

  SIM800_GSM_STATE_DISABLED,
  SIM800_GSM_STATE_INITIALIZATION,
  SIM800_GSM_STATE_NETWORK_NOT_REGISTERED,
  SIM800_GSM_STATE_ERROR,
  SIM800_GSM_STATE_READY,
  SIM800_GSM_STATE_CALL,
  SIM800_GSM_STATE_SMS,
  SIM800_STATE_READING_SMS,

} sim800GsmState_t;

typedef enum sim800SmsState_ {
  SIM800_SMS_STATE_UNDEFINED = 0,
  SIM800_SMS_STATE_IDLE,
  SIM800_SMS_STATE_TRANSMIT_AWAITING_PROMPT,
  SIM800_SMS_STATE_TRANSMIT_PROCEEDING,
  SIM800_SMS_STATE_TRANSMIT_AWAITING_RESULT,
  SIM800_SMS_STATE_TRANSMIT_SUCCESS,
  SIM800_SMS_STATE_TRANSMIT_ERROR,
  SIM800_SMS_STATE_RECEIVE_,

} sim800SmsState_t;

typedef struct sim800GsmSms_ {
  sim800SmsState_t State;

  char phone[64 + 1]; // 16 * 4 + '\0'
  char msg[320 + 1];  // 70 * 4 + '\0'

  size_t len;
  size_t idx;

  int smsIdxInMem;

} sim800GsmSms_t;

typedef struct sim800GsmUssd_ {
  // TODO ...

} sim800GsmUssd_t;

typedef enum sim800CallDir_ {
  SIM800_GSM_CALL_DIR_OUTGOING = 0,
  SIM800_GSM_CALL_DIR_INCOMING,
} sim800CallDir_t;

typedef enum sim800CallState_ {
  SIM800_GSM_CALL_STATE_ACTIVE = 0,
  SIM800_GSM_CALL_STATE_HELD,
  SIM800_GSM_CALL_STATE_DIALING,  // outgoing
  SIM800_GSM_CALL_STATE_ALERTING, // outgoing
  SIM800_GSM_CALL_STATE_INCOMING, // incoming
  SIM800_GSM_CALL_STATE_WAITING,  // incoming
  SIM800_GSM_CALL_STATE_DISCONNECT,
} sim800CallState_t;

typedef enum sim800CallMode_ {
  SIM800_GSM_CALL_MODE_VOICE = 0,
  SIM800_GSM_CALL_MODE_DATA,
  SIM800_GSM_CALL_MODE_FAX,
} sim800CallMode_t;

typedef enum sim800CallPhoneType_ {
  SIM800_GSM_CALL_TYPE_UNKNOWN = 129,
  SIM800_GSM_CALL_TYPE_NATIONAL = 161,
  SIM800_GSM_CALL_TYPE_INTERNATIONAL = 145,
  SIM800_GSM_CALL_TYPE_NETWORK_SPEC = 177,
} sim800CallPhoneType_t;

typedef struct sim800GsmCall_ {
  sim800CallState_t State;

  bool hangup;

  char *phone;
  int rings;
  uint32_t ts;

} sim800GsmCall_t;

typedef enum sim800GsmDtmfCode_ {
  SIM800_DTMF_CODE_0 = 0,
  SIM800_DTMF_CODE_1,
  SIM800_DTMF_CODE_2,
  SIM800_DTMF_CODE_3,
  SIM800_DTMF_CODE_4,
  SIM800_DTMF_CODE_5,
  SIM800_DTMF_CODE_6,
  SIM800_DTMF_CODE_7,
  SIM800_DTMF_CODE_8,
  SIM800_DTMF_CODE_9,
  SIM800_DTMF_CODE_STAR,
  SIM800_DTMF_CODE_HASH,
  SIM800_DTMF_CODE_A,
  SIM800_DTMF_CODE_B,
  SIM800_DTMF_CODE_C,
  SIM800_DTMF_CODE_D,

} sim800GsmDtmfCode_t;

typedef struct sim800GsmDtmf_ {
  sim800GsmDtmfCode_t Code;
  int duration;

  char bufer[20];

} sim800GsmDtmf_t;

typedef struct sim800Gsm_ {
  struct {
    int mnc;
    int stat;
    char lac[5];
    char ci[5];
  } Network;

  sim800GsmSms_t Sms;
  sim800GsmUssd_t Ussd;

  sim800GsmCall_t Call;
  sim800GsmDtmf_t Dtmf;

  sim800GsmState_t State;
  int balance;

} sim800Gsm_t;

#endif /* SIM800_GSM_TYPES_H_ */
