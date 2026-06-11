/*
 * motor.c
 *
 *  Created on: Mar 17, 2026
 *      Author: barbe
 */

#ifndef SRC_MOTOR_C_
#define SRC_MOTOR_C_

#include "motor.h"

void setMotor(enum MOTOR_TYPE choice)
{
	if (choice == LEFT)
		HAL_GPIO_WritePin(LEFT_MOTOR_PORT, LEFT_MOTOR_PIN, GPIO_PIN_SET);
	else if (choice == RIGHT)
		HAL_GPIO_WritePin(RIGHT_MOTOR_PORT, RIGHT_MOTOR_PIN, GPIO_PIN_SET);
	else if (choice == BOTH)
	{
		HAL_GPIO_WritePin(LEFT_MOTOR_PORT, LEFT_MOTOR_PIN, GPIO_PIN_SET);
		HAL_GPIO_WritePin(RIGHT_MOTOR_PORT, RIGHT_MOTOR_PIN, GPIO_PIN_SET);
	}
}

void resetMotor(enum MOTOR_TYPE choice)
{
	if (choice == LEFT)
		HAL_GPIO_WritePin(LEFT_MOTOR_PORT, LEFT_MOTOR_PIN, GPIO_PIN_RESET);
	else if (choice == RIGHT)
		HAL_GPIO_WritePin(RIGHT_MOTOR_PORT, RIGHT_MOTOR_PIN, GPIO_PIN_RESET);
	else if (choice == BOTH)
	{
		HAL_GPIO_WritePin(LEFT_MOTOR_PORT, LEFT_MOTOR_PIN, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(RIGHT_MOTOR_PORT, RIGHT_MOTOR_PIN, GPIO_PIN_RESET);
	}
}

void toggleMotor(enum MOTOR_TYPE choice)
{
  	if (choice == LEFT)
  		HAL_GPIO_TogglePin(LEFT_MOTOR_PORT, LEFT_MOTOR_PIN);
	else if (choice == RIGHT)
		HAL_GPIO_TogglePin(RIGHT_MOTOR_PORT, RIGHT_MOTOR_PIN);
}

static uint32_t motor_end_left  = 0;
static uint32_t motor_end_right = 0;

void setMotorTimed(enum MOTOR_TYPE choice, int time)
{
    uint32_t end = HAL_GetTick() + time;

    if (choice == LEFT || choice == BOTH)
    {
        HAL_GPIO_WritePin(LEFT_MOTOR_PORT, LEFT_MOTOR_PIN, GPIO_PIN_SET);
        motor_end_left = end;
    }
    if (choice == RIGHT || choice == BOTH)
    {
        HAL_GPIO_WritePin(RIGHT_MOTOR_PORT, RIGHT_MOTOR_PIN, GPIO_PIN_SET);
        motor_end_right = end;
    }
}

void setMotorTimed_Update(void)
{
    uint32_t now = HAL_GetTick();

    if (motor_end_left > 0 && now >= motor_end_left)
    {
        HAL_GPIO_WritePin(LEFT_MOTOR_PORT, LEFT_MOTOR_PIN, GPIO_PIN_RESET);
        motor_end_left = 0;
    }
    if (motor_end_right > 0 && now >= motor_end_right)
    {
        HAL_GPIO_WritePin(RIGHT_MOTOR_PORT, RIGHT_MOTOR_PIN, GPIO_PIN_RESET);
        motor_end_right = 0;
    }
}

bool isMotorTimedActive(enum MOTOR_TYPE choice)
{
    if (choice == LEFT)  return motor_end_left > 0;
    if (choice == RIGHT) return motor_end_right > 0;
    if (choice == BOTH)  return motor_end_left > 0 || motor_end_right > 0;
    return false;
}

#endif /* SRC_MOTOR_C_ */
