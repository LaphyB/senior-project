/*
 * led.h
 *
 *  Created on: Mar 17, 2026
 *      Author: barbe
 */

#ifndef SRC_LED_H_
#define SRC_LED_H_

#include "stm32wbxx_hal.h"

#include "pins.h"

void setLED();

void resetLED();

void toggleLED();

#endif /* SRC_LED_H_ */
