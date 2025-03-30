/*
 * sim800_impl.h
 *
 *  Created on: Sep 22, 2021
 *      Author: sa100
 */

#ifndef SIM800_CONF_H_
#define SIM800_CONF_H_


#define SIM800_MODULES_COUNT	1
#define SIM800_MODULE_INDEX_MIN	0
#define SIM800_MODULE_INDEX_MAX	(SIM800_MODULES_COUNT - 1)

#define SIM800_PARSERS_MAX		16
#define SIM800_WILDCARD_CHR		'\e'

//#define SIM800_WITH_RTOS		1

#define SIM800_DEFAULT_TIMEOUT	1000
#define SIM800_COMMAND_TIMEOUT_MS		1000


#if SIM800_WITH_RTOS
#include "cmsis_os.h"
#define SIM800_DELAY_MS(x)	osDelay(x)
#else
#define SIM800_DELAY_MS(x)	HAL_Delay(x)
#endif


#include "stm32f0xx_hal.h"
#include "stm32f0xx_ll_usart.h"

#include "main.h"

#define SIM800_GET_TICK()					HAL_GetTick()
#define SIM800_USART						USART2


#define SIM800_RXNE_IT_ENABLE()				LL_USART_EnableIT_RXNE(SIM800_USART)
#define SIM800_RXNE_IT_DISABLE()			LL_USART_DisableIT_RXNE(SIM800_USART)

#define SIM800_TXE_IT_ENABLE()				LL_USART_EnableIT_TXE(SIM800_USART)
#define SIM800_TXE_IT_DISABLE()				LL_USART_DisableIT_TXE(SIM800_USART)

#define SIM800_GET_USART_TC_FLAG()			LL_USART_IsActiveFlag_TC(SIM800_USART)
#define SIM800_GET_USART_TXE_FLAG()			LL_USART_IsActiveFlag_TXE(SIM800_USART)
#define SIM800_GET_USART_RXNE_FLAG()		LL_USART_IsActiveFlag_RXNE(SIM800_USART)

#define SIM800_TRANSMIT_BYTE(b)				LL_USART_TransmitData8(SIM800_USART, b)
#define SIM800_RECEIVE_BYTE()				LL_USART_ReceiveData8(SIM800_USART)

#define SIM800_POWER_ENABLE_PIN_SET()		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_SET)
#define SIM800_POWER_ENABLE_PIN_RESET()		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET)

#define SIM800_PWRKEY_PIN_SET()				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, GPIO_PIN_SET)
#define SIM800_PWRKEY_PIN_RESET()			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, GPIO_PIN_RESET)

#define SIM800_POWER_ON()	do {									\
								SIM800_POWER_ENABLE_PIN_SET();		\
								SIM800_PWRKEY_PIN_SET();			\
								SIM800_DELAY_MS(5000);				\
							} while(0)

#define SIM800_POWER_OFF()	do {									\
								SIM800_TXE_IT_DISABLE();			\
								SIM800_RXNE_IT_DISABLE();			\
								SIM800_PWRKEY_PIN_RESET();			\
								SIM800_POWER_ENABLE_PIN_RESET();	\
							} while(0)

#define SIM800_RESET()		do {									\
								SIM800_TXE_IT_DISABLE();			\
								SIM800_RXNE_IT_DISABLE();			\
								SIM800_PWRKEY_PIN_RESET();			\
								SIM800_DELAY_MS(1000);				\
								SIM800_PWRKEY_PIN_SET();			\
								SIM800_RXNE_IT_ENABLE();			\
							} while(0)



#endif /* SIM800_CONF_H_ */
