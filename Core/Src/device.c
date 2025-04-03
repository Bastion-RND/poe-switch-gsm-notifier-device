#include <string.h>
#include <stdbool.h>

#include "device.h"

static bool check_phone_number(char *phone_number) {
  bool result = false;
  if (strlen(phone_number) == 12 && phone_number[0] == '+') {
    char* phoneNumPtr = &phone_number[1];
    result = true;
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

bool check_comma(char* text) {
  return (*text == ',');
}

bool parse_action(char* text, bool* action) {
  bool result = false;
  if (*text == '1') {
    *action = true;
    result = true;
  } else if (*text == '0') {
    *action = false;
    result = true;
  }
  return result;
}

static void cmd_a_parse(char* phone_number, char* text) {
  bool action = false;
  int stage = 0;
  char* txtPtr = text;
  if (strlen(text) != 2) return;
  if (!check_phone_number(phone_number)) return;
  while (stage < 2) {
    switch (stage) {
      case 0:
        if (!check_comma(txtPtr)) {return;}
      break;

      case 1:
        if (!parse_action(txtPtr, &action)) {return;}
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
  bool action = false;
  int stage = 0;
  char* txtPtr = text;
  if (strlen(text) != 15) return;
  while (stage < 4) {
    switch (stage) {
      case 0:
      case 2:
        if (!check_comma(txtPtr)) {return;}
      break;

      case 1:
        if (!parse_action(txtPtr, &action)) {return;}
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

void command_parser(char* phone_number, char* text) {
  debug_printf("[Device] command parser start, phone: %s, text: %s\n", phone_number, text);
  char* textPtr = text;
  if (strlen(text) > 1 && *textPtr == '$') {
    textPtr++;
    char cmd = *textPtr;
    textPtr++;
    switch (cmd) {
      case 'a':
        cmd_a_parse(phone_number, textPtr);
        break;
      case 'b':
        cmd_b_parse(textPtr);
         break;
      case 'l':
          debug_printf("[Device] command <l> - get all binded numbers\n");
        break;
      case 'c':
       debug_printf("[Device] command <c> - config upload\n");
       break;
      case 'g':
       debug_printf("[Device] command <g> - get config\n");
       break;
      case 'r':
       debug_printf("[Device] command <r> - set relay\n");
       break;
    }
  }
}
