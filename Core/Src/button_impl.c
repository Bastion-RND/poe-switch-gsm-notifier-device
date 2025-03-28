#include "main.h"
#include "button.h"

#include "sim800.h"

extern sim800_t* SIM800;

/**
 * EXAMPLE for hardware button and pseudo button by STLED316S
 * Low-level pin initialization for discrete inputs
 * It is performed by the ID received through the pointer
 */
bool
button_lowlevel_init(Button_t *p)
{
  bool result = false;
  if(p)
  {
    /* Initialized in HAL */
    result = true;
  }
  return result;
}

/**
 * EXAMPLE for hardware button and pseudo button by STLED316S
 */
bool
button_get_input(Button_t *p)
{
  bool b;

  switch(p->id)
  {
    case BUTTON_SEND_SMS_ID:
      b = HAL_GPIO_ReadPin(BUTTON_SEND_SMS_PORT, BUTTON_SEND_SMS_PIN);
      break;

    default:
      b = false;
  }
  return (!b == p->Level);
}


static int right_button = 0;
static int left_button = 0;


uint32_t
button_get_tick()
{
  return HAL_GetTick();
}

void
button_pressed_long_callback(Button_t *p) {
  switch(p->id) {
  case BUTTON_SEND_SMS_ID:
  debug_printf("Try send SMS\n");
  sim800_sms_send(SIM800, "+79094294096", "Hello");
    break;
  }
}
