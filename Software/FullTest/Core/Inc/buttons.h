/*
 * buttons.h
 *
 *  Created on: Mar 17, 2026
 *      Author: barbe
 */

#ifndef INC_BUTTONS_H_
#define INC_BUTTONS_H_

#include "stm32wbxx_hal.h"

#include "pins.h"

typedef union
{
    struct
    {
        uint8_t BUTTON1 : 1;
        uint8_t BUTTON2 : 1;
        uint8_t BUTTON3 : 1;
        uint8_t BUTTON4 : 1;
        uint8_t BUTTON5 : 1;
        uint8_t BUTTON6 : 1;
        uint8_t JOYSTICKBUTTON1 : 1;
        uint8_t JOYSTICKBUTTON2 : 1;
    };
    uint8_t allButtons;
} ButtonStates_t;

ButtonStates_t ReadButtons(void);

#endif /* INC_BUTTONS_H_ */
