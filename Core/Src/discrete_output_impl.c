#include "main.h"
#include "discrete_output.h"

bool
discrete_output_ll_init(DiscreteOutput_t* p) {
  bool result = false;
  if(p) {
    switch(p->id) {
        case RELAY_1_ID: /* init by HAL */
        case RELAY_2_ID: /* init by HAL */
        case USER_LED_ID: /* init by HAL */
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
discrete_output_ll_set_level(DiscreteOutput_t *p, bool level) {
  if(p) {
    bool outputNewState;
    outputNewState = (level == p->Level);
    switch(p->id) {
      case RELAY_1_ID:
      if(outputNewState)	{HAL_GPIO_WritePin(RELAY_1_GPIO_Port, RELAY_1_Pin, GPIO_PIN_SET);}
      else	{HAL_GPIO_WritePin(RELAY_1_GPIO_Port, RELAY_1_Pin, GPIO_PIN_RESET);}
        break;

      case RELAY_2_ID:
      if(outputNewState)	{HAL_GPIO_WritePin(RELAY_2_GPIO_Port, RELAY_2_Pin, GPIO_PIN_SET);}
      else	{HAL_GPIO_WritePin(RELAY_2_GPIO_Port, RELAY_2_Pin, GPIO_PIN_RESET);}
        break;

      case USER_LED_ID:
      if(outputNewState)	{HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, GPIO_PIN_SET);}
      else	{HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, GPIO_PIN_RESET);}
        break;

      default:
        break;
    }
  }
}

uint32_t
discrete_output_ll_get_tick() {
  return HAL_GetTick();
}
