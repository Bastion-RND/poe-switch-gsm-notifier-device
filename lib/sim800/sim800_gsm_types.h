#ifndef SIM800_GSM_TYPES_H_
#define SIM800_GSM_TYPES_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum sim800GsmState_ {
  SIM800_GSM_STATE_UNDEFINED = 0,
  SIM800_GSM_STATE_INITIALIZATION,
  SIM800_GSM_STATE_NOT_REGISTERED,
  SIM800_GSM_STATE_SMS,
  SIM800_GSM_STATE_READY,
  SIM800_GSM_STATE_ERROR,
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

typedef struct sim800Sms_ {
  sim800SmsState_t State;

  char phone[64 + 1]; // 16 * 4 + '\0'
  char message[320 + 1];  // 70 * 4 + '\0'

  size_t len;
  size_t idx;

  int smsIdxInMem;

} sim800Sms_t;

typedef struct Sim800Gsm_ {
  struct {
    int mnc;
    int stat;
    char lac[5];
    char ci[5];
  } Network;
  sim800GsmState_t State;
  sim800Sms_t Sms;
  int balance;
} Sim800Gsm_t;

#endif /* SIM800_GSM_TYPES_H_ */
