/*
 * buttons.c
 *
 *  Created on: Mar 17, 2026
 *      Author: barbe
 */

#include "buttons.h"

ButtonStates_t  ReadButtons(void)
{
	ButtonStates_t buttons = {0};

	buttons.BUTTON1 = (HAL_GPIO_ReadPin(BUTTON1_PORT, BUTTON1_PIN) == GPIO_PIN_SET);
	buttons.BUTTON2 = (HAL_GPIO_ReadPin(BUTTON2_PORT, BUTTON2_PIN) == GPIO_PIN_SET);
	buttons.BUTTON3 = (HAL_GPIO_ReadPin(BUTTON3_PORT, BUTTON3_PIN) == GPIO_PIN_SET);
	buttons.BUTTON4 = (HAL_GPIO_ReadPin(BUTTON4_PORT, BUTTON4_PIN) == GPIO_PIN_SET);
	buttons.BUTTON5 = (HAL_GPIO_ReadPin(BUTTON5_PORT, BUTTON5_PIN) == GPIO_PIN_SET);
	buttons.BUTTON6 = (HAL_GPIO_ReadPin(BUTTON6_PORT, BUTTON6_PIN) == GPIO_PIN_SET);

	buttons.JOYSTICKBUTTON1 = (HAL_GPIO_ReadPin(JOYSTICK_BUTTON1_PORT, JOYSTICK_BUTTON1_PIN) == GPIO_PIN_SET);
	buttons.JOYSTICKBUTTON2 = (HAL_GPIO_ReadPin(JOYSTICK_BUTTON2_PORT, JOYSTICK_BUTTON2_PIN) == GPIO_PIN_SET);

	return buttons;
}

