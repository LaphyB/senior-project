/*
 * motor.h
 *
 *  Created on: Mar 17, 2026
 *      Author: barbe
 */

#ifndef SRC_MOTOR_H_
#define SRC_MOTOR_H_

#include "stm32wbxx_hal.h"
#include <stdbool.h>

#include "pins.h"

enum MOTOR_TYPE
{
	LEFT,
	RIGHT,
	BOTH
};

void setMotor(enum MOTOR_TYPE choice);

void resetMotor(enum MOTOR_TYPE choice);

void toggleMotor(enum MOTOR_TYPE choice);

void setMotorTimed(enum MOTOR_TYPE choice, int time);

void setMotorTimed_Update(void);

bool isMotorTimedActive(enum MOTOR_TYPE choice);

#endif /* SRC_MOTOR_H_ */
