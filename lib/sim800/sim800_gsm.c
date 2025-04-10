#include <stdio.h>
#include <string.h>

#include "sim800.h"
#include "utf8_xcoder.h"

static void switch_gsm_state(Sim800Handle_t *, sim800GsmState_t);
static void switch_sms_state(Sim800Handle_t *, sim800SmsState_t);
static void gsm_init_process(Sim800Handle_t *, Sim800Event_t, void *);

bool flag = false;  // FIXME fix initialization procedure

static void no_carrier_parser(Sim800Handle_t *p, const char *str, void *param) {
  debug_printf("< %s > %s\n", __func__, str);
  switch_gsm_state(p, SIM800_GSM_STATE_READY);
}

/**
 * Parses the incoming CMTI (new SMS message) indication from the SIM800 module.
 *
 * This function handles the transition to the SMS reading state when a new SMS
 * message is indicated. If a call is currently hanging up, it unlocks the
 * SIM800 and sends the hangup command. It also updates the ring count and
 * triggers a GSM ring callback.
 *
 * @param p Pointer to the SIM800 instance structure.
 * @param str The string received from the SIM800 module containing the CMTI
 * indication.
 * @param param Additional parameters for future use (currently unused).
 */
// static void cmti_parser(Sim800Handle_t* p, const char *str, void *param) {
//   if (p->Gsm.State != SIM800_STATE_READING_SMS) {
//     debug_printf("CMTI handler called");
//     // char str[128];
//     // snprintf(str, sizeof(str), "AT+CMGDA=\"DEL ALL\"\n");
//     // sim800_cmd(p, str, 1000, gsm_init_process, NULL, SIM800_FLOW_ASYNC);
//     const char *prefix = "+CMTI: \"SM\",";
//     size_t prefix_len = strlen(prefix);
//     if (strncmp(str, prefix, prefix_len) != 0) {
//       return;
//     }
//     const char *index_str = str + prefix_len;
//     int index = atoi(index_str);
//     p->Gsm.Sms.smsIdxInMem = index;
//     switch_gsm_state(p, SIM800_STATE_READING_SMS);
//   }
// }

static void cmgl_parser(Sim800Handle_t *p, const char *str, void *param) {
  debug_printf("< %s > %s\n", __func__, str);
}

static void sms_send_prompt_parser(Sim800Handle_t *p, const char *str, void *param) {
  (void) str;
  (void) param;
  if (p && p->Gsm.Sms.State == SIM800_SMS_STATE_TRANSMIT_AWAITING_PROMPT) {
    switch_sms_state(p, SIM800_SMS_STATE_TRANSMIT_PAYLOAD);

    while (p->Gsm.Sms.Send.charIdx < p->Gsm.Sms.Send.len) {
      while (circular_buf_full(p->TxCbufHandle)) {}
      circular_buf_put(p->TxCbufHandle, p->Gsm.Sms.Send.message[p->Gsm.Sms.Send.charIdx++]);
      SIM800_TXE_IT_ENABLE(); /* Transmit data */
    }
    /* finish SMS text */
    while (circular_buf_full(p->TxCbufHandle)) {}
    circular_buf_put(p->TxCbufHandle, 0x1A); /* Ctrl-Z */
    switch_sms_state(p, SIM800_SMS_STATE_TRANSMIT_AWAITING_RESULT);
  }
}

static void sms_send_result_callback(Sim800Handle_t *p, Sim800Event_t ev, void *param) {
  (void) param;
  switch (ev) {
    case SIM800_EVENT_CMD_RESULT_OK:
      switch_sms_state(p, SIM800_SMS_STATE_TRANSMIT_SUCCESS);
      break;

    case SIM800_EVENT_CMD_RESULT_ERR:
    case SIM800_EVENT_CMD_RESULT_TIMEOUT:
      switch_sms_state(p, SIM800_SMS_STATE_TRANSMIT_ERROR);
      break;

    default:
      break;
  }
}

static void switch_gsm_state(Sim800Handle_t *p, sim800GsmState_t NewGsmState) {
  switch (NewGsmState) {
  case SIM800_GSM_STATE_UNDEFINED:
    debug_printf("[GSM] switch state to UNDEFINED\n");
    memset(&p->Gsm, 0x00, sizeof(Sim800Gsm_t));
    break;

  case SIM800_GSM_STATE_INITIALIZATION:
    debug_printf("[GSM] switch state to INITIALIZATION\n");
    // sim800_parser_add(p, "+CREG: ", creg_parser, NULL);
    // sim800_parser_add(p, "SMS Ready", sms_ready_parser, NULL);
    // switch_sms_state(p, SIM800_SMS_STATE_IDLE);
    // gsm_init_process(p, SIM800_EVENT_INITIALIZATION_BEGIN, NULL);
    break;

  case SIM800_GSM_STATE_ERROR:
    debug_printf("[GSM] switch state to ERROR\n");
    break;

  case SIM800_GSM_STATE_READY:
    debug_printf("[GSM] switch state to READY\n");
    sim800_parser_remove(p, "NO CARRIER");
    break;

  default:
    break;
  }
  p->Gsm.State = NewGsmState;
}

static void switch_sms_state(Sim800Handle_t *p, sim800SmsState_t NewSmsState) {
  switch (NewSmsState) {
  case SIM800_SMS_STATE_IDLE:
    memset(&p->Gsm.Sms, 0x00, sizeof(sim800Sms_t));
    break;

  case SIM800_SMS_STATE_TRANSMIT_AWAITING_PROMPT:
    sim800_parser_add(p, "> ", sms_send_prompt_parser, NULL);
    switch_gsm_state(p, SIM800_GSM_STATE_SMS);
    break;

  case SIM800_SMS_STATE_TRANSMIT_PAYLOAD:
    sim800_parser_remove(p, "> ");
    break;

  case SIM800_SMS_STATE_TRANSMIT_AWAITING_RESULT:
    sim800_parser_add(p, "+CMGS: ", NULL, NULL);
    break;

  case SIM800_SMS_STATE_TRANSMIT_ERROR:
    if (p->Gsm.Sms.Send.callback != NULL) {
      p->Gsm.Sms.Send.callback(SIM800_EVENT_SEND_SMS_ERROR);
      sim800_parser_remove(p, "+CMGS: ");
      sim800_parser_remove(p, "> ");
      switch_gsm_state(p, SIM800_GSM_STATE_READY);
    }
    break;

  case SIM800_SMS_STATE_TRANSMIT_SUCCESS:
    if (p->Gsm.Sms.Send.callback != NULL) {
      p->Gsm.Sms.Send.callback(SIM800_EVENT_SEND_SMS_SUCCESS);
    }
    sim800_parser_remove(p, "+CMGS: ");
    sim800_parser_remove(p, "> ");
    switch_gsm_state(p, SIM800_GSM_STATE_READY);
    break;
  }
  if (NewSmsState == SIM800_SMS_STATE_TRANSMIT_SUCCESS || NewSmsState == SIM800_SMS_STATE_TRANSMIT_ERROR) {
    NewSmsState = SIM800_SMS_STATE_IDLE;
  }
  p->Gsm.Sms.State = NewSmsState;
}

void sim800_gsm_run(Sim800Handle_t *p) {
  static uint32_t ts_every_second = 0;
  static uint32_t ts_every_10_seconds = 0;
  static uint32_t ts_every_30_seconds = 0;
  static uint32_t ts_every_minute = 0;

  char str[128];

  switch (p->Gsm.State) {
  case SIM800_GSM_STATE_UNDEFINED:
    switch_gsm_state(p, SIM800_GSM_STATE_INITIALIZATION);
    break;

  case SIM800_GSM_STATE_INITIALIZATION:
    // if (!flag) {
    //   if (!sim800_is_locked(p) && p->Gsm.Sms.State == SIM800_SMS_STATE_IDLE) {
    //     sim800_cmd(p, "AT+CSMP=17,167,0,25\n", 1000, NULL, NULL, SIM800_FLOW_ASYNC);
    //     flag = true;
    //   }
    // }
      if (p->Gsm.Sms.State == SIM800_SMS_STATE_IDLE) {
        switch_gsm_state(p, SIM800_GSM_STATE_READY); // FIXME
      }
    break; /* event-driven waiting */

  case SIM800_GSM_STATE_NOT_REGISTERED:
    /* every minute */
    if ((SIM800_GET_TICK() - ts_every_minute) >= 60 * 1000) {
      ts_every_minute = SIM800_GET_TICK();

      /* Soft reset module */
      sim800_cmd(p, "AT+CFUN=1,1\n", 1000, NULL, NULL, SIM800_FLOW_SYNC);

      /* Reinitialize GSM */
      switch_gsm_state(p, SIM800_GSM_STATE_UNDEFINED);
    }
    break;

    case SIM800_STATE_READY:
      if (!flag) {
        if (!sim800_is_locked(p) && p->Gsm.Sms.State == SIM800_SMS_STATE_IDLE) {
          sim800_cmd(p, "AT+CSMP=17,167,0,25\n", 1000, NULL, NULL, SIM800_FLOW_ASYNC);
          flag = true;
        }
      }
    break;

  case SIM800_GSM_STATE_SMS:
  default:
    break;
  }
}

bool sim800_sms_send(Sim800Handle_t* p, char* phone, char* message, sim800_sms_callback_t cb) {
  if (p == NULL || phone == NULL || message == NULL) {
    return false;
  }
  if (strlen(message) > 256) {
    return false;
  }
  // if (p->Gsm.State == SIM800_GSM_STATE_READY && p->Gsm.State == SIM800_GSM_STATE_UNDEFINED) {
  //   p->Gsm.Sms.State = SIM800_SMS_STATE_IDLE;
  // }
  if (p->Gsm.Sms.State != SIM800_SMS_STATE_IDLE || sim800_is_locked(p) || !flag) {
    return false;
  }
  debug_printf("Sending SMS, len %d", strlen(message));

  char str[strlen(phone) * 4 + 11 /* AT+CMGS=""\n */ + 1 /* \0 */];

  int code;
  char *dstStrPtr;

  p->Gsm.Sms.Send.callback = cb;

  dstStrPtr = &p->Gsm.Sms.Send.phone[0];
  while ((code = decode_code_point(&phone))) {
    code_point_to_str(&dstStrPtr, code);
  }
  dstStrPtr = &p->Gsm.Sms.Send.message[0];
  while ((code = decode_code_point(&message))) {
    // if (dstStrPtr - &p->Gsm.Sms.Send.message[0] >= 320) {
    //   debug_printf("Overflow");
    //   break;
    // }
    code_point_to_str(&dstStrPtr, code);
  }
  p->Gsm.Sms.Send.len = strlen(p->Gsm.Sms.Send.message);
  p->Gsm.Sms.Send.charIdx = 0;

  switch_sms_state(p, SIM800_SMS_STATE_TRANSMIT_AWAITING_PROMPT);
  snprintf(str, sizeof(str), "AT+CMGS=\"%s\"\r", p->Gsm.Sms.Send.phone);
  sim800_cmd(p, str, 60 * 1000, sms_send_result_callback, NULL, SIM800_FLOW_ASYNC);
  return true;
}
