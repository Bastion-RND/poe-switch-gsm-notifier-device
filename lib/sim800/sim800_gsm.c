#include <stdio.h>
#include <string.h>

#include "sim800.h"
#include "utf8_xcoder.h"

static void switch_gsm_state(Sim800Handle_t *, sim800GsmState_t);
static void switch_sms_state(Sim800Handle_t *, sim800SmsState_t);
static void gsm_init_process(Sim800Handle_t *, Sim800Event_t, void *);

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

// static void cmgr_parser(Sim800Handle_t* p, const char *str, void *param) {
//   if (p->Gsm.State != SIM800_STATE_READING_SMS) {
//     char text[128];
//     sim800_readline(p, text, sizeof(text), 1000);
//     char *ptr = &text[0];
//
//     debug_printf("Text SMS raw: %s\n", text);
//
//     debug_printf("CMGR handler called, %s\n, text: ", str);
//
//     int counter = 20;
//
//     while (counter) {
//       int code_point = str_to_code_point(&ptr);
//       if (code_point < 0) {
//         debug_printf("Error decoding code point\n");
//         break;
//       }
//       counter -= 4;
//       debug_printf("\nCounter = %d\n", counter);
//       // Преобразуем кодовый пункт в символы UTF-16LE и выводим
//       // В UTF-16LE младший байт идет первым, поэтому меняем порядок байтов
//       char utf16_char[2];
//       utf16_char[0] = (code_point >> 0) & 0xFF;  // Младший байт
//       utf16_char[1] = (code_point >> 8) & 0xFF; // Старший байт
//
//       // Выводим символы
//       debug_printf("%c%c", utf16_char[0], utf16_char[1]);
//     }
//     debug_printf("\n");
//     switch_gsm_state(p, SIM800_GSM_STATE_READY);
//   }
// }

static void cmgl_parser(Sim800Handle_t *p, const char *str, void *param) {
  debug_printf("< %s > %s\n", __func__, str);
}

static void sms_send_prompt_parser(Sim800Handle_t *p, const char *str,
                                   void *param) {
  if (p && p->Gsm.Sms.State == SIM800_SMS_STATE_TRANSMIT_AWAITING_PROMPT) {
    switch_sms_state(p, SIM800_SMS_STATE_TRANSMIT_PROCEEDING);

    while (p->Gsm.Sms.idx < p->Gsm.Sms.len) {
      while (circular_buf_full(p->TxCbufHandle))
        ;
      circular_buf_put(p->TxCbufHandle, p->Gsm.Sms.msg[p->Gsm.Sms.idx++]);
      SIM800_TXE_IT_ENABLE(); /* Transmit data */
    }
    /* finish SMS text */
    while (circular_buf_full(p->TxCbufHandle))
      ;
    circular_buf_put(p->TxCbufHandle, 0x1A);

    switch_sms_state(p, SIM800_SMS_STATE_TRANSMIT_AWAITING_RESULT);
  }
}

static void sms_send_result_parser(Sim800Handle_t *p, const char *str,
                                   void *param) {
  char res[32];
  sim800_readline(p, res, sizeof(res), 1000); // skip "\r\n"
  sim800_readline(p, res, sizeof(res), 1000); // "OK" or "ERROR"

  if (strstr(res, "OK")) {
    switch_sms_state(p, SIM800_SMS_STATE_TRANSMIT_SUCCESS);
  } else {
    switch_sms_state(p, SIM800_SMS_STATE_TRANSMIT_ERROR);
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
    switch_sms_state(p, SIM800_SMS_STATE_IDLE);
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

  case SIM800_SMS_STATE_TRANSMIT_AWAITING_RESULT:
    sim800_parser_add(p, "+CMGS: ", sms_send_result_parser, NULL);
    break;

  case SIM800_SMS_STATE_TRANSMIT_ERROR:
  case SIM800_SMS_STATE_TRANSMIT_SUCCESS:
    sim800_parser_remove(p, "+CMGS: ");
    sim800_parser_remove(p, "> ");
    switch_gsm_state(p, SIM800_GSM_STATE_READY);
    break;

  default:
    break;
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

  case SIM800_GSM_STATE_READY:
    // if (p->Gsm.Sms.State != SIM800_SMS_STATE_IDLE) return;
    // uint32_t curr_time = SIM800_GET_TICK();
    // if (p->Gsm.Network.stat && !p->Gsm.Network.mnc) {
    //   /* Request GSM network operator code (mnc) */
    //   sim800_parser_add(p, RESPONSE_GSM_OPERATOR, cops_parser, NULL);
    //   sim800_cmd(p, "AT+COPS?\n", 1000, NULL, NULL, SIM800_FLOW_ASYNC);
    // }
    // if (curr_time - ts_every_second >= 1 * 1000) {
    //   if (sim800_cmd(p, "AT+CNMI=2,1\n", 1000, NULL, NULL, SIM800_FLOW_ASYNC) ==
    //       SIM800_RESULT_OK) {
    //     ts_every_second = SIM800_GET_TICK();
    //   }
    // }
    // if (curr_time - ts_every_minute >= 60 * 1000) {
    //   if (sim800_cmd(p, "AT+CMGDA=\"ALL\"\n", 1000, NULL, NULL, SIM800_FLOW_ASYNC) ==
    //       SIM800_RESULT_OK) {
    //     debug_printf("\n[GSM] removing all messages in memory\n");
    //     ts_every_minute = SIM800_GET_TICK();
    //       }
    // }
    break;

  default:
    break;
  }
}

void sim800_gsm_network_restart(Sim800Handle_t *p) {
  switch_gsm_state(p, SIM800_GSM_STATE_UNDEFINED);
}

bool sim800_sms_send(Sim800Handle_t *p, char *phone, char *message) {
  if (p == NULL) {
    return false;
  }

  char str[strlen(phone) * 4 + 11 /* AT+CMGS=""\n */ + 1 /* \0 */];

  char *ptr;
  int code;

  uint32_t ts;

  if (p && p->Gsm.State == SIM800_GSM_STATE_READY && phone && message) {
    if (p->Gsm.Sms.State != SIM800_SMS_STATE_IDLE) {
      switch_sms_state(p, SIM800_SMS_STATE_IDLE);
    }

    ptr = &p->Gsm.Sms.phone[0];
    while ((code = decode_code_point(&phone))) {
      code_point_to_str(&ptr, code);
    }

    /* NOTE: Set parser! b4 command */
    switch_sms_state(p, SIM800_SMS_STATE_TRANSMIT_AWAITING_PROMPT);
    SIM800_DELAY_MS(100);

    snprintf(str, sizeof(str), "AT+CMGS=\"%s\"\r", p->Gsm.Sms.phone);
    if (sim800_cmd(p, str, 10 * 1000, NULL, NULL, SIM800_FLOW_ASYNC) ==
        SIM800_RESULT_OK) {
      ptr = &p->Gsm.Sms.msg[0];
      while ((code = decode_code_point(&message))) {
        code_point_to_str(&ptr, code);
      }
      p->Gsm.Sms.len = strlen(p->Gsm.Sms.msg);
      p->Gsm.Sms.idx = 0;

      ts = SIM800_GET_TICK();

      /* NOTE: Blocking till sending result! */
      // uint32_t attempt_counter = 0;
      // while (p->Gsm.State != SIM800_GSM_STATE_READY)
      // {
      //     attempt_counter++;
      //     if ((SIM800_GET_TICK() - ts) >= 10 * 1000) {
      //     	// Attempt to finalize
      //     	while(circular_buf_full(p->TxCbufHandle));
      //         circular_buf_put(p->TxCbufHandle, 0x1A);
      //
      //         switch_sms_state(p, SIM800_SMS_STATE_TRANSMIT_ERROR);
      //     }
      //     else {
      //         if (attempt_counter % 10 == 0) {
      //             debug_printf("Awaiting SMS sending result...\n");
      //         }
      //         SIM800_DELAY_MS(100);
      //     }
      // }
    } else {
      switch_sms_state(p, SIM800_SMS_STATE_TRANSMIT_ERROR);
    }
  }
  return (p->Gsm.Sms.State == SIM800_SMS_STATE_TRANSMIT_SUCCESS);
}
