/*
 * gesture_detector.h
 *
 *  Created on: Apr 20, 2026
 *      Author: barbe
 */

#ifndef INC_GESTURE_DETECTOR_H_
#define INC_GESTURE_DETECTOR_H_

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "config.h"
#include "sensor_data.h"

typedef enum {
    GESTURE_NONE = 0,
    GESTURE_FLICK_LEFT,
    GESTURE_FLICK_RIGHT,
    GESTURE_FLICK_UP,
    GESTURE_FLICK_DOWN,
    GESTURE_ROTATE_CW,
    GESTURE_ROTATE_CCW
} GestureType_t;

typedef enum
{
    STATE_IDLE = 0,
    STATE_DETECTING,
    STATE_DEBOUNCE,
    STATE_COOLDOWN
} GestureState_t;

void GestureDetector_Init(void);

GestureType_t GestureDetector_Update(SensorData_t *data);

GestureState_t GestureDetector_GetState(void);

float GestureDetector_GetPeakX(void);

float GestureDetector_GetPeakY(void);

float GestureDetector_GetPeakZ(void);

float GestureDetector_GetPeakMagnitude(void);

const char* GestureDetector_Gesturetostring(GestureType_t gesture);

const char* GestureDetector_StateToString(GestureState_t state);

#endif /* INC_GESTURE_DETECTOR_H_ */
