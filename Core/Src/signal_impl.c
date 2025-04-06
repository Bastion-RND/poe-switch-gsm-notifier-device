#include "main.h"
#include "discrete_output.h"

bool
signal_lowlevel_init(Signal_t *pDiscreteOutput) {
  bool result = false;

  if(pDiscreteOutput) {
    switch(pDiscreteOutput->id) {
      case SIGNAL_RELAY_1_ID: /* init by HAL */
        result = true;
        break;

      case SIGNAL_RELAY_2_ID: /* init by HAL */
        result = true;
        break;

      case SIGNAL_USER_LED_ID: /* init by HAL */
        result = true;
        break;

      default:
        /* error ID */
        break;
    }
  }
  return result;
}

void
signal_set_output(Signal_t *pDiscreteOutput, bool level) {
  if(pDiscreteOutput) {
    bool outputNewState;
    outputNewState = (level == pDiscreteOutput->Level);
    switch(pDiscreteOutput->id) {
      case SIGNAL_RELAY_1_ID:
      if(outputNewState)	{HAL_GPIO_WritePin(RELAY_1_GPIO_Port, RELAY_1_Pin, GPIO_PIN_SET);}
        else	{HAL_GPIO_WritePin(RELAY_1_GPIO_Port, RELAY_1_Pin, GPIO_PIN_RESET);}
        break;

      case SIGNAL_RELAY_2_ID:
      if(outputNewState)	{HAL_GPIO_WritePin(RELAY_2_GPIO_Port, RELAY_2_Pin, GPIO_PIN_SET);}
        else	{HAL_GPIO_WritePin(RELAY_2_GPIO_Port, RELAY_2_Pin, GPIO_PIN_RESET);}
        break;

      case SIGNAL_USER_LED_ID:
      if(outputNewState)	{HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, GPIO_PIN_SET);}
        else	{HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, GPIO_PIN_RESET);}
        break;

      default:
        /* error ID */
        break;
    }
  }
}

uint32_t
signal_get_tick() {
  return HAL_GetTick();
}
