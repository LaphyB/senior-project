/*
 * led.c
 *
 *  Created on: Mar 17, 2026
 *      Author: barbe
 */

#include "led.h"

void setLED()
{
	HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
}

void resetLED()
{
	HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
}

void toggleLED()
{
  	HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
}
