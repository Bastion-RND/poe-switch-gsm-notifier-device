/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f0xx_hal.h"
#include "stm32f0xx_ll_usart.h"
#include "stm32f0xx_ll_rcc.h"
#include "stm32f0xx_ll_bus.h"
#include "stm32f0xx_ll_cortex.h"
#include "stm32f0xx_ll_system.h"
#include "stm32f0xx_ll_utils.h"
#include "stm32f0xx_ll_pwr.h"
#include "stm32f0xx_ll_gpio.h"
#include "stm32f0xx_ll_dma.h"

#include "stm32f0xx_ll_exti.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "SEGGER_RTT.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
#if defined(DEBUG) && !defined(NDEBUG)
#define debug_printf(...) SEGGER_RTT_printf(0, ##__VA_ARGS__)
#else
#define debug_printf(...)
#endif // DEBUG

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define SEND_SMS_BTN_Pin GPIO_PIN_13
#define SEND_SMS_BTN_GPIO_Port GPIOC
#define ADC_AKB_Pin GPIO_PIN_0
#define ADC_AKB_GPIO_Port GPIOA
#define ADC_220_Pin GPIO_PIN_1
#define ADC_220_GPIO_Port GPIOA
#define RESET_BTN_Pin GPIO_PIN_8
#define RESET_BTN_GPIO_Port GPIOA
#define USER_LED_Pin GPIO_PIN_9
#define USER_LED_GPIO_Port GPIOA
#define SIM800_RESET_Pin GPIO_PIN_11
#define SIM800_RESET_GPIO_Port GPIOA
#define TAMPER_BTN_Pin GPIO_PIN_12
#define TAMPER_BTN_GPIO_Port GPIOA
#define RELAY_2_Pin GPIO_PIN_6
#define RELAY_2_GPIO_Port GPIOB
#define RELAY_1_Pin GPIO_PIN_7
#define RELAY_1_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
