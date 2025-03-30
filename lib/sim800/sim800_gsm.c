#include <stdio.h>
#include <string.h>

#include "sim800.h"
#include "utf8_xcoder.h"

static char phone[16];

/* internal function prototypes */
static void switch_gsm_state(Sim800Handle_t*, sim800GsmState_t);
static void switch_sms_state(Sim800Handle_t*, sim800SmsState_t);
static void switch_call_state(Sim800Handle_t*, sim800SmsState_t);
static void gsm_init_process(Sim800Handle_t*, Sim800Event_t, void *);
static void gsm_state_machine(Sim800Handle_t*);

__attribute__((weak)) void on_gsm_ring_callback(Sim800Handle_t* p,
                                                sim800GsmCall_t *p1) {
  (void)p;
  (void)p1;
}

static void creg_parser(Sim800Handle_t* p, const char *str, void *param) {
  const char *ptr = str + strlen("+CREG: ");

  // '+CREG: 1,"66C6","0638"\r\n'
  // TODO: hex parser

  p->GsmNetwork.Network.stat = sim800_parse_int(&ptr);
  ptr += 2; /* skip [ ," ] */

  sim800_parse_str(&ptr, &p->GsmNetwork.Network.lac[0]);
  ptr += 3; /* skip [ "," ] */

  sim800_parse_str(&ptr, &p->GsmNetwork.Network.ci[0]);

  if (p->GsmNetwork.Network.stat == 1) {
    switch_gsm_state(p, SIM800_GSM_STATE_READY);
  } else {
    switch_gsm_state(p, SIM800_GSM_STATE_NETWORK_NOT_REGISTERED);
  }
}

static void dtmf_parser(Sim800Handle_t* p, const char *str, void *param) {
  const char *ptr = str + strlen(DTMF_DETECT);

  // '+DTMF: 5,320'

  if (*ptr >= '0' && *ptr <= '9') {
    p->GsmNetwork.Dtmf.Code = *ptr - '0';
  } else if (*ptr >= 'A' && *ptr <= 'D') {
    p->GsmNetwork.Dtmf.Code = *ptr - 'A' + 12;
  } else if (*ptr == '*') {
    p->GsmNetwork.Dtmf.Code = SIM800_DTMF_CODE_STAR;
  } else if (*ptr == '#') {
    p->GsmNetwork.Dtmf.Code = SIM800_DTMF_CODE_HASH;
  } else {
    return; /* ERROR */
  }

  ptr += 2; /* skip [ <CODE>, ] */
  p->GsmNetwork.Dtmf.duration = sim800_parse_int(&ptr);

  on_gsm_dtmf_callback(p, &p->GsmNetwork.Dtmf);
}

// static void
// on_call_start(sim800_t* p) {
//     printf("Incoming call. GPRS Disabled\n");
//     gprs_disable(p);
//
//     if (_sim800_is_cmd_locked(p)) {
//         _sim800_cmd_unlock(p);
//         printf("Warning: Command FINALIZE!\n");
//
//         /* Finalize broken command */
//         if (p->Command.callback) {
//             p->Command.callback(
//                 p, SIM800_EVENT_COMMAND_RESULT_ERROR,
//                 p->Command.callback_param
//             );
//         }
//     }
//     p->Gsm.Call.ts = SIM800_GET_TICK();
// }
//
// static void
// on_call_end(sim800_t* p) {
//     if (p->Gsm.Call.State == SIM800_GSM_CALL_STATE_DISCONNECT) {
//         printf("[GSM CLCC] Call finished. GPRS Enabled\n");
//         sim800_parser_remove(p, DTMF_DETECT);
//         p->Gsm.Call.rings = 0;
//         gprs_enable(p);
//     }
// }

static void no_carrier_parser(Sim800Handle_t* p, const char *str, void *param) {
  debug_printf("< %s > %s\n", __func__, str);
  switch_gsm_state(p, SIM800_GSM_STATE_READY);
  p->GsmNetwork.Call.rings = 0;
  p->GsmNetwork.Call.ts = 0;
}

static void ring_parser(Sim800Handle_t* p, const char *str, void *param) {
  if (p->GsmNetwork.State != SIM800_GSM_STATE_CALL) {
    memset(phone, 0x00, sizeof(phone));
    switch_gsm_state(p, SIM800_GSM_STATE_CALL);
    debug_printf("< %s > incoming call!\n", __func__);
  }

  if (p->GsmNetwork.Call.hangup) {
    sim800_unlock(p);
    sim800_cmd(p, "ATH\n", 10 * 1000, NULL, NULL, SIM800_FLOW_SYNC);
    switch_gsm_state(p, SIM800_GSM_STATE_READY);

  } else {
    debug_printf("< %s > %s %d\n", __func__, str, p->GsmNetwork.Call.rings);
    p->GsmNetwork.Call.rings += 1;
  }

  on_gsm_ring_callback(p, &p->GsmNetwork.Call);
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
static void cmti_parser(Sim800Handle_t* p, const char *str, void *param) {
  if (p->GsmNetwork.State != SIM800_STATE_READING_SMS) {
    debug_printf("CMTI handler called");
    // char str[128];
    // snprintf(str, sizeof(str), "AT+CMGDA=\"DEL ALL\"\n");
    // sim800_cmd(p, str, 1000, gsm_init_process, NULL, SIM800_FLOW_ASYNC);
    const char *prefix = "+CMTI: \"SM\",";
    size_t prefix_len = strlen(prefix);
    if (strncmp(str, prefix, prefix_len) != 0) {
      return;
    }
    const char *index_str = str + prefix_len;
    int index = atoi(index_str);
    p->GsmNetwork.Sms.smsIdxInMem = index;
    switch_gsm_state(p, SIM800_STATE_READING_SMS);
  }
}

static void cmgr_parser(Sim800Handle_t* p, const char *str, void *param) {
  if (p->GsmNetwork.State != SIM800_STATE_READING_SMS) {
    char text[128];
    sim800_readline(p, text, sizeof(text), 1000);
    char *ptr = &text[0];

    debug_printf("Text SMS raw: %s\n", text);

    debug_printf("CMGR handler called, %s\n, text: ", str);

    int counter = 20;

    while (counter) {
      int code_point = str_to_code_point(&ptr);
      if (code_point < 0) {
        debug_printf("Error decoding code point\n");
        break;
      }
      counter -= 4;
      debug_printf("\nCounter = %d\n", counter);
      // Преобразуем кодовый пункт в символы UTF-16LE и выводим
      // В UTF-16LE младший байт идет первым, поэтому меняем порядок байтов
      char utf16_char[2];
      utf16_char[0] = (code_point >> 0) & 0xFF;  // Младший байт
      utf16_char[1] = (code_point >> 8) & 0xFF; // Старший байт

      // Выводим символы
      debug_printf("%c%c", utf16_char[0], utf16_char[1]);
    }
    debug_printf("\n");
    switch_gsm_state(p, SIM800_GSM_STATE_READY);
  }
}

// static void
// clip_parser(sim800_t* p, const char* str, void* param)
//{
//     const char* ptr = str + strlen("+CLIP: ");
//
//     // '+CLIP: "+79950913375",145,"",0,"",0'
//
//     sim800_parse_str(&ptr, &p->Gsm.Call.phone[0]);
//     ptr += 2; /* skip [ ", ] */
//
//     p->Gsm.Call.Type = sim800_parse_int(&ptr);
//     ptr += 1; /* skip [ " ] */
//
//     //    sim800_skip_parse(p, 2 /* [ ," ] */);
//     //    sim800_str_parser(p, &str[0]);
//     //
//     //    sim800_skip_parse(p, 2 /* [ ", ] */);
//     //    sim800_num_parser(p, &num);
//     //
//     //    sim800_skip_parse(p, 2 /* [ ," ] */);
//     //    sim800_str_parser(p, &p->Gsm.Call.name);
//     //
//     //    sim800_skip_parse(p, 2 /* [ ", ] */);
//     //    sim800_num_parser(p, &num);
//     //
//     //    sim800_skip_parse(p, 3 /* [ "\r\n ] */);
//
//     //    if (p->Gsm.Call.rings == 0) {
//     //    	p->Gsm.Call.rings += 1;
//     //        on_call_start(p);
//     //    }
//     //    else {
//     //        p->Gsm.Call.rings++;
//     //    }
//     //    on_gsm_call_callback(p, &p->Gsm.Call);
//
//     //sim800_cmd(p, "AT+CLCC\n", 1000, NULL, NULL, SIM800_FLOW_SYNC);
//
//     switch (p->Gsm.Call.FSM)
//     {
//         case SIM800_GSM_CALL_FSM_IDLE:
//         case SIM800_GSM_CALL_FSM_UNKNOWN:
//         case SIM800_GSM_CALL_FSM_FINISHED:
//             p->Gsm.Call.FSM = SIM800_GSM_CALL_FSM_INCOMING_RING;
//             p->Gsm.Call.rings = 0;
//             on_call_start(p);
//             break;
//
//         case SIM800_GSM_CALL_FSM_INCOMING_RING:
//             p->Gsm.Call.rings += 1;
//             break;
//     }
//     //    printf("[GLIP]: %s\n", str);
//     on_gsm_call_callback(p, &p->Gsm.Call);
// }

static void clcc_parser(Sim800Handle_t* p, const char *str, void *param) {
  const char *ptr = str + strlen("+CLCC: ");

  sim800CallDir_t CallDirection;
  sim800CallState_t CallState;
  sim800CallMode_t CallMode;
  sim800CallPhoneType_t CallPhoneType;
  int num;

  // '+CLCC: 1,1,6,0,0,"+79950913375",145,""'
  debug_printf("< %s > [CLCC]: %s\n", __func__, ptr);

  num = sim800_parse_int(&ptr);
  ptr += 1; /* skip [ , ] */

  // p->Gsm.Call.Direction = sim800_parse_int(&ptr);
  CallDirection = sim800_parse_int(&ptr);
  ptr += 1; /* skip [ , ] */

  // p->Gsm.Call.State = sim800_parse_int(&ptr);
  CallState = sim800_parse_int(&ptr);
  ptr += 1; /* skip [ , ] */

  // p->Gsm.Call.Mode = sim800_parse_int(&ptr);
  CallMode = sim800_parse_int(&ptr);
  ptr += 1; /* skip [ , ] */

  num = sim800_parse_int(&ptr); // <mpty>
  ptr += 2;                     /* skip [ ," ] */

  sim800_parse_str(&ptr, phone);
  ptr += 2; /* skip [ ", ] */

  // p->Gsm.Call.Type = sim800_parse_int(&ptr);
  CallPhoneType = sim800_parse_int(&ptr);
  ptr += 1; /* skip [ " ] */

  if (CallDirection == SIM800_GSM_CALL_DIR_INCOMING) {
    if (p->GsmNetwork.State != SIM800_GSM_STATE_CALL) {
      switch_gsm_state(p, SIM800_GSM_STATE_CALL);
    }
    switch (CallState) {
    case SIM800_GSM_CALL_STATE_ACTIVE:
      if (p->GsmNetwork.Call.ts == 0) {
        debug_printf("< %s > answered: %s\n", __func__, phone);
        on_gsm_call_callback(p, &p->GsmNetwork.Call);
        p->GsmNetwork.Call.ts = SIM800_GET_TICK();
      }
      break;

    case SIM800_GSM_CALL_STATE_INCOMING:
      debug_printf("< %s > incoming: %s\n", __func__, phone);
      break;

    case SIM800_GSM_CALL_STATE_DISCONNECT:
      switch_gsm_state(p, SIM800_GSM_STATE_READY);
      debug_printf("< %s > disconnect: %s\n", __func__, phone);
      on_gsm_call_callback(p, &p->GsmNetwork.Call);
      break;

    default:
      debug_printf("< %s > unknown state: %d\n", __func__, CallState);
    }

    if (p->GsmNetwork.Call.hangup) {
      sim800_unlock(p);
      sim800_cmd(p, "ATH\n", 10 * 1000, NULL, NULL, SIM800_FLOW_SYNC);
      switch_gsm_state(p, SIM800_GSM_STATE_READY);
    }
  }

  //    sim800_skip_parse(p, 2 /* [ ," ] */);
  //    sim800_str_parser(p, &p->Gsm.Call.name);
  //
  //    sim800_skip_parse(p, 3 /* [ "\r\n ] */);

  //    if (p->Gsm.Call.State == SIM800_GSM_CALL_STATE_INCOMING)
  //    {
  //        on_call_start(p);
  //    }
  //    if (p->Gsm.Call.State == SIM800_GSM_CALL_STATE_DISCONNECT)
  //    {
  //        on_call_end(p);
  //    }
  //    on_gsm_call_callback(p, &p->Gsm.Call);

  //    switch (p->Gsm.Call.FSM)
  //    {
  //        case SIM800_GSM_CALL_FSM_IDLE:
  //        case SIM800_GSM_CALL_FSM_UNKNOWN:
  //        case SIM800_GSM_CALL_FSM_FINISHED:
  //            if (p->Gsm.Call.State == SIM800_GSM_CALL_STATE_INCOMING)
  //            {
  //                p->Gsm.Call.FSM = SIM800_GSM_CALL_FSM_INCOMING_RING;
  //                p->Gsm.Call.rings = 0;
  //                on_call_start(p);
  //            }
  //            break;
  //
  //        case SIM800_GSM_CALL_FSM_INCOMING:
  //        case SIM800_GSM_CALL_FSM_INCOMING_RING:
  //            if (p->Gsm.Call.State == SIM800_GSM_CALL_STATE_DISCONNECT)
  //            {
  //                p->Gsm.Call.FSM = SIM800_GSM_CALL_FSM_FINISHED;
  //                on_call_end(p);
  //            }
  //            break;
  //    }

  //    printf("[GLCC]: %s\n", str);
  //    on_gsm_call_callback(p, &p->Gsm.Call);
}

static void cops_parser(Sim800Handle_t* p, const char *str, void *param) {
  const char *ptr = str + strlen(RESPONSE_GSM_OPERATOR);
  int num;

  // '+COPS: 0,2,"25099"'

  num = sim800_parse_int(&ptr); // <mode>
  ptr += 1;                     /* skip [ , ] */

  num = sim800_parse_int(&ptr); // <format>
  ptr += 2;                     /* skip [ ," ] */

  p->GsmNetwork.Network.mnc = sim800_parse_int(&ptr);
  ptr += 1; /* skip [ , ] */

  sim800_parser_remove(p, RESPONSE_GSM_OPERATOR);
}

static void cusd_parser(Sim800Handle_t* p, const char *str, void *param) {
  const char *ptr = str + strlen("+CUSD: ");
  const char *msg;
  int num;

  // '+CUSD: 0, "....", 72'

  num = sim800_parse_int(&ptr);
  ptr += 3; /* skip [ ,_" ] */

  msg = ptr;

  while (!(*(ptr - 2) == '"' && *(ptr - 1) == ',' && *(ptr) == ' ') &&
         *ptr != '\0') {
    ptr++;
  }
  num = sim800_parse_int(&ptr); // <dcs>

  on_gsm_ussd_callback(p, msg, num);
}

/**
 *
 */
static void sms_send_prompt_parser(Sim800Handle_t* p, const char *str, void *param) {
  if (p && p->GsmNetwork.Sms.State == SIM800_SMS_STATE_TRANSMIT_AWAITING_PROMPT) {
    switch_sms_state(p, SIM800_SMS_STATE_TRANSMIT_PROCEEDING);

    while (p->GsmNetwork.Sms.idx < p->GsmNetwork.Sms.len) {
      while (circular_buf_full(p->TxCbufHandle))
        ;
      circular_buf_put(p->TxCbufHandle, p->GsmNetwork.Sms.msg[p->GsmNetwork.Sms.idx++]);
      SIM800_TXE_IT_ENABLE(); /* Transmit data */
    }
    /* finish SMS text */
    while (circular_buf_full(p->TxCbufHandle))
      ;
    circular_buf_put(p->TxCbufHandle, 0x1A);

    switch_sms_state(p, SIM800_SMS_STATE_TRANSMIT_AWAITING_RESULT);
  }
}

/**
 *
 */
static void sms_send_result_parser(Sim800Handle_t* p, const char *str, void *param) {
  char res[32];
  sim800_readline(p, res, sizeof(res), 1000); // skip "\r\n"
  sim800_readline(p, res, sizeof(res), 1000); // "OK" or "ERROR"

  if (strstr(res, "OK")) {
    switch_sms_state(p, SIM800_SMS_STATE_TRANSMIT_SUCCESS);
  } else {
    switch_sms_state(p, SIM800_SMS_STATE_TRANSMIT_ERROR);
  }
}

static void switch_gsm_state(Sim800Handle_t* p, sim800GsmState_t NewGsmState) {
  switch (NewGsmState) {
  case SIM800_GSM_STATE_UNDEFINED:
    debug_printf("[GSM] switch state to UNDEFINED\n");
    memset(&p->GsmNetwork, 0x00, sizeof(Sim800GsmNetwork_t));
    break;

  case SIM800_GSM_STATE_INITIALIZATION:
    debug_printf("[GSM] switch state to INITIALIZATION\n");
    /* default */
    sim800_parser_add(p, "+CREG: ", creg_parser, NULL);
    /* TODO: if call enabled */
    sim800_parser_add(p, "+CLCC: ", clcc_parser, NULL);
    sim800_parser_add(p, "RING", ring_parser, NULL);
    /* TODO: if ussd enabled */
    sim800_parser_add(p, "+CUSD: ", cusd_parser, NULL);

    sim800_parser_add(p, "+CMTI: ", cmti_parser, NULL);
    sim800_parser_add(p, "+CMGR: ", cmgr_parser, NULL);
    /* temporary */

    memset(phone, 0x00, sizeof(phone));
    p->GsmNetwork.Call.phone = phone;

    switch_sms_state(p, SIM800_SMS_STATE_IDLE);

    gsm_init_process(p, SIM800_EVENT_INITIALIZATION_BEGIN, 0);
    break;

  case SIM800_GSM_STATE_CALL:
    debug_printf("[GSM] switch state to CALL\n");
    sim800_parser_add(p, "NO CARRIER", no_carrier_parser, NULL);
    sim800_parser_add(p, DTMF_DETECT, dtmf_parser, NULL);

    p->GsmNetwork.Call.hangup = false;
    p->GsmNetwork.Call.rings = 0;
    p->GsmNetwork.Call.ts = 0;

    sim800_unlock(p);
    break;

  case SIM800_GSM_STATE_SMS:
    debug_printf("[GSM] switch state to SMS\n");
    break;

  case SIM800_STATE_READING_SMS:
    debug_printf("[GSM] switch state to READING_SMS\n");
    break;

  case SIM800_GSM_STATE_ERROR:
    debug_printf("[GSM] switch state to ERROR\n");
    break;

  case SIM800_GSM_STATE_READY:
    debug_printf("[GSM] switch state to READY\n");
    sim800_parser_remove(p, "NO CARRIER");
    sim800_parser_remove(p, DTMF_DETECT);
    // gprs_enable(p);
    break;

  default:
    break;
  }
  p->GsmNetwork.State = NewGsmState;
  on_gsm_state_callback(p, p->GsmNetwork.State);
}

/**
 *
 */
static void switch_sms_state(Sim800Handle_t *p, sim800SmsState_t NewSmsState) {
  switch (NewSmsState) {
  case SIM800_SMS_STATE_IDLE:
    memset(&p->GsmNetwork.Sms, 0x00, sizeof(sim800GsmSms_t));
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
  p->GsmNetwork.Sms.State = NewSmsState;
}

/* init sequence ------------------------------------------------- */

static void gsm_init_process(Sim800Handle_t* p, Sim800Event_t ev, void *param) {
  static int stage;
  static int errCounter = 0;
  char str[128];

  switch (ev) {
  case SIM800_EVENT_INITIALIZATION_BEGIN:
    stage = 0; /* start process */
    break;

  case SIM800_EVENT_CMD_RESULT_ERR:
    errCounter++;
    if (errCounter >= 10) {
      switch_gsm_state(p, SIM800_GSM_STATE_ERROR);
      return;
    }
    SIM800_DELAY_MS(500); /* wait, last step... */
    break;

  case SIM800_EVENT_CMD_RESULT_OK:
    stage++; /* switch to next step */
    break;

  default: /* retry last step */
    break;
  }

  switch (stage) {
  case 0:
    debug_printf("[GSM] init stage 0: reset to factory defaults\n");
    sim800_cmd(p, "AT&F0\n", 1000, gsm_init_process, NULL, SIM800_FLOW_ASYNC);
    break;

  case 1:
    debug_printf("[GSM] init stage 1: set RINGs before answer\n");
    sim800_cmd(p, "ATS0=2\n", 1000, gsm_init_process, NULL, SIM800_FLOW_ASYNC);
    break;

  case 2:
    debug_printf("[GSM] init stage 2: enable DTMF detection\n");
    snprintf(str, sizeof(str), "AT+DDET=%d,%d,%d,%d\n", 1, 0, 1, 0);
    sim800_cmd(p, str, 1000, gsm_init_process, NULL, SIM800_FLOW_ASYNC);
    break;

    //        case 2: /* Enable Calling Line Identification Presentation */
    //            snprintf(str, sizeof(str), "AT+CLIP=%d\n", 1);
    //            sim800_cmd(p, str, 1000, gsm_init_process, NULL,
    //            SIM800_FLOW_ASYNC); break;

  case 3: /* Set network registration info format */
    debug_printf("[GSM] init stage 3: set network registration info format\n");
    snprintf(str, sizeof(str), "AT+CREG=%d\n", 2);
    sim800_cmd(p, str, 1000, gsm_init_process, NULL, SIM800_FLOW_ASYNC);
    break;

  case 4:
    debug_printf("[GSM] init stage 4: set local timestamp mode\n");
    snprintf(str, sizeof(str), "AT+CLTS=%d\n", 1);
    sim800_cmd(p, str, 1000, gsm_init_process, NULL, SIM800_FLOW_ASYNC);
    break;

  case 5:
    debug_printf("[GSM] init stage 5: set operator selection\n");
    snprintf(str, sizeof(str), "AT+COPS=%d,%d\n", 0, 2);
    sim800_cmd(p, str, 1000, gsm_init_process, NULL, SIM800_FLOW_ASYNC);
    break;

    // case 6:
    //     debug_printf("[GSM] stage 6: set SMS magic text format\n");
    //     snprintf(str, sizeof(str), "AT+CSMP=17,167,0,25\n"); //,25
    //     sim800_cmd(p, str, 1000, gsm_init_process, NULL, SIM800_FLOW_ASYNC);
    //     break;

  case 6:
    debug_printf("[GSM] init stage 6: set SMS text mode\n");
    snprintf(str, sizeof(str), "AT+CMGF=%d\n", 1);
    sim800_cmd(p, str, 1000, gsm_init_process, NULL, SIM800_FLOW_ASYNC);
    break;

  case 7:
    debug_printf("[GSM] init stage 7: set SMS text encoding\n");
    snprintf(str, sizeof(str), "AT+CSCS=\"%s\"\n", "UCS2");
    sim800_cmd(p, str, 1000, gsm_init_process, NULL, SIM800_FLOW_ASYNC);
    break;

  case 8:
    debug_printf("[GSM] init stage 8: disable NetLight LED\n");
    snprintf(str, sizeof(str), "AT+CNETLIGHT=%d\n", 0);
    sim800_cmd(p, str, 1000, gsm_init_process, NULL, SIM800_FLOW_ASYNC);
    break;

  // case 9:
  //   debug_printf("[GSM] init stage 9: delete all SMS\n");
  //   snprintf(str, sizeof(str), "AT+CMGDA=\"DEL ALL\"\n");
  //   sim800_cmd(p, str, 1000, gsm_init_process, NULL, SIM800_FLOW_ASYNC);
  //   break;

  default:
    debug_printf("[GSM] init stage 9: ready\n");
    switch_gsm_state(p, SIM800_GSM_STATE_READY);
    break;
  }
}

/**
 *
 */
static void gsm_state_machine(Sim800Handle_t* p) {
  static uint32_t ts_every_second = 0;
  static uint32_t ts_every_10_seconds = 0;
  static uint32_t ts_every_30_seconds = 0;
  static uint32_t ts_every_minute = 0;

  char str[32];

  switch (p->GsmNetwork.State) {
  case SIM800_GSM_STATE_UNDEFINED:
    debug_printf("[GSM] State: UNDEFINED\n");
    switch_gsm_state(p, SIM800_GSM_STATE_INITIALIZATION);
    break;

  case SIM800_GSM_STATE_INITIALIZATION:
    break;

  case SIM800_GSM_STATE_NETWORK_NOT_REGISTERED:
    /* every minute */
    if ((SIM800_GET_TICK() - ts_every_minute) >= 60 * 1000) {
      ts_every_minute = SIM800_GET_TICK();

      /* Soft reset module */
      sim800_cmd(p, "AT+CFUN=1,1\n", 1000, NULL, NULL, SIM800_FLOW_SYNC);

      /* Reinitialize GSM */
      switch_gsm_state(p, SIM800_GSM_STATE_UNDEFINED);
    }
    break;

  case SIM800_GSM_STATE_CALL:
    if ((SIM800_GET_TICK() - ts_every_second) >= 1 * 1000) {
      ts_every_second = SIM800_GET_TICK();

      if (p->GsmNetwork.Call.rings >= 1) {
        sim800_cmd(p, "AT+CLCC\n", 1000, NULL, NULL, SIM800_FLOW_ASYNC);
      }
    }
    break;

  case SIM800_GSM_STATE_SMS:
    //            if (!_sim800_is_cmd_locked(p))
    //                switch_gsm_state(p, SIM800_GSM_STATE_READY);
    break;

  case SIM800_STATE_READING_SMS:
    char temp_str[128];
    snprintf(temp_str, sizeof(temp_str), "AT+CMGR=%d\n", p->GsmNetwork.Sms.smsIdxInMem);
    sim800_cmd(p, temp_str, 1000, NULL, NULL,
               SIM800_FLOW_ASYNC); // read SMS by index
    sim800_cmd(p, "AT+CMGD=1,4\n", 1000, NULL, NULL,
               SIM800_FLOW_ASYNC); // remove all SMS
    switch_gsm_state(p, SIM800_GSM_STATE_READY);
    break;

  case SIM800_GSM_STATE_READY:
    /* GPRS */
    // sim800_gprs_run(p);

    if (!sim800_is_locked(p)) {
      if (p->GsmNetwork.Network.stat && !p->GsmNetwork.Network.mnc) {
        /* Request GSM network operator code (mnc) */
        sim800_parser_add(p, RESPONSE_GSM_OPERATOR, cops_parser, NULL);
        sim800_cmd(p, "AT+COPS?\n", 1000, NULL, NULL, SIM800_FLOW_ASYNC);
      }
      if ((SIM800_GET_TICK() - ts_every_second) >= 1 * 1000) {
        ts_every_second = SIM800_GET_TICK();
        sim800_cmd(p, "AT+CNMI=2,1\n", 1000, NULL, NULL, SIM800_FLOW_ASYNC);
        // sim800_cmd(p, "AT+CSDH=1\n", 1000, NULL, NULL, SIM800_FLOW_ASYNC);
      }
      if (SIM800_GET_TICK() - ts_every_10_seconds >= 10 * 1000) {
        ts_every_10_seconds = SIM800_GET_TICK();
      }
      if (SIM800_GET_TICK() - ts_every_30_seconds >= 30 * 1000) {
        ts_every_30_seconds = SIM800_GET_TICK();
      }
    }
    break;

  default:
    break;
  }
}

/**
 *
 */
void sim800_gsm_run(Sim800Handle_t* p) {
  /* Update SIM800 GSM FSM */
  gsm_state_machine(p);

  /* DO NOT block other threads! */
  SIM800_DELAY_MS(0);
}

/**
 *
 */
bool sim800_call_answer(Sim800Handle_t* p) {
  if (p && p->GsmNetwork.State == SIM800_GSM_STATE_READY) {
    return (sim800_cmd(p, "ATA\n", 1000, NULL, NULL, SIM800_FLOW_ASYNC) ==
            SIM800_RESULT_OK);
  }
  return false;
}

/**
 *
 */
bool sim800_call_hangup(Sim800Handle_t* p) {
  if (p && p->GsmNetwork.State == SIM800_GSM_STATE_CALL) {
    p->GsmNetwork.Call.hangup = true;
    return true;
  }
  return false;
}

/**
 *
 */
bool sim800_ussd_request(Sim800Handle_t* p, char *request) {
  char encoded_ussd[strlen(request) * 4 + 1];
  char str[16 * sizeof(encoded_ussd)];

  char *ptr;
  int code;

  if (request && p && p->GsmNetwork.State == SIM800_GSM_STATE_READY) {
    ptr = &encoded_ussd[0];
    while ((code = decode_code_point(&request))) {
      code_point_to_str(&ptr, code);
    }
    snprintf(str, sizeof(str), "AT+CUSD=%d,\"%s\"\n", 1, encoded_ussd);
    return (sim800_cmd(p, str, 10 * 1000, NULL, NULL, SIM800_FLOW_ASYNC) ==
            SIM800_RESULT_OK);
  }
  return false;
}

/**
 *
 */
bool sim800_sms_send(Sim800Handle_t* p, char *phone, char *message) {
  char str[strlen(phone) * 4 + 11 /* AT+CMGS=""\n */ + 1 /* \0 */];

  char *ptr;
  int code;

  uint32_t ts;

  if (p && p->GsmNetwork.State == SIM800_GSM_STATE_READY && phone && message) {
    if (p->GsmNetwork.Sms.State != SIM800_SMS_STATE_IDLE) {
      switch_sms_state(p, SIM800_SMS_STATE_IDLE);
    }

    ptr = &p->GsmNetwork.Sms.phone[0];
    while ((code = decode_code_point(&phone))) {
      code_point_to_str(&ptr, code);
    }

    /* NOTE: Set parser! b4 command */
    switch_sms_state(p, SIM800_SMS_STATE_TRANSMIT_AWAITING_PROMPT);
    SIM800_DELAY_MS(100);

    snprintf(str, sizeof(str), "AT+CMGS=\"%s\"\r", p->GsmNetwork.Sms.phone);
    if (sim800_cmd(p, str, 10 * 1000, NULL, NULL, SIM800_FLOW_ASYNC) ==
        SIM800_RESULT_OK) {
      ptr = &p->GsmNetwork.Sms.msg[0];
      while ((code = decode_code_point(&message))) {
        code_point_to_str(&ptr, code);
      }
      p->GsmNetwork.Sms.len = strlen(p->GsmNetwork.Sms.msg);
      p->GsmNetwork.Sms.idx = 0;

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
  return (p->GsmNetwork.Sms.State == SIM800_SMS_STATE_TRANSMIT_SUCCESS);
}

/* callback's ---------------------------------------------------- */

__attribute__((weak)) void on_gsm_state_callback(Sim800Handle_t* p,
                                                 sim800GsmState_t State) {
  // debug_printf("[GSM] ");
  // switch (State)
  // {
  //     case SIM800_GSM_STATE_UNDEFINED:
  //         debug_printf("UNDEFINED!\n");
  //         break;
  //
  //     case SIM800_GSM_STATE_DISABLED:
  //         debug_printf("Disabled\n");
  //         break;
  //
  //     case SIM800_GSM_STATE_INITIALIZATION:
  //         debug_printf("Initialization...\n");
  //         break;
  //
  //     case SIM800_GSM_STATE_NETWORK_NOT_REGISTERED:
  //         debug_printf("Is not registered in network\n");
  //         break;
  //
  //     case SIM800_GSM_STATE_CALL:
  //         debug_printf("Voice call\n");
  //         break;
  //
  //     case SIM800_GSM_STATE_READY:
  //         debug_printf("Ready\n");
  //         break;
  //
  //     case SIM800_GSM_STATE_ERROR:
  //         debug_printf("ERROR!\n");
  //         break;
  //
  //     default:
  //         break;
  // }
}

__attribute__((weak)) void on_gsm_call_callback(Sim800Handle_t* p,
                                                sim800GsmCall_t *pc) {
  //    uint32_t delta;

  //    printf("[GSM CALL] ");
  //    switch (p->Gsm.Call.State)
  //    {
  //        case SIM800_GSM_CALL_STATE_INCOMING:
  //            if (p->Gsm.Call.rings == 0) {
  //                printf("Incoming: '%s'\n", p->Gsm.Call.phone);
  //            }
  //            else {
  //                printf("'%s', Rings: %d\n", p->Gsm.Call.phone,
  //                p->Gsm.Call.rings);
  //            }
  //            break;
  //
  //        case SIM800_GSM_CALL_STATE_DISCONNECT:
  //        	p->Gsm.Call.rings = 0;
  //            delta = SIM800_GET_TICK() - p->Gsm.Call.ts;
  //            printf("Finished: '%s', time: %d ms\n", p->Gsm.Call.phone,
  //            delta); break;
  //
  //        default:
  //            printf("'%s' state: %d\n", p->Gsm.Call.phone,
  //            p->Gsm.Call.State); break;
  //    }
}

__attribute__((weak)) void on_gsm_dtmf_callback(Sim800Handle_t* p,
                                                sim800GsmDtmf_t *pd) {
  //    printf("DTMF Detected: '");
  //    switch (pd->Code) {
  //        case SIM800_DTMF_CODE_STAR:
  //            printf("*");
  //            break;
  //        case SIM800_DTMF_CODE_HASH:
  //            printf("#");
  //            break;
  //        default:
  //            printf("%d", pd->Code);
  //    }
  //    printf("', %d ms\n", pd->duration);
}

__attribute__((weak)) void on_gsm_ussd_callback(Sim800Handle_t* p, const char *msg,
                                                int format) {
  //    printf("USSD: '%s', format %d\n", msg, format);
}
