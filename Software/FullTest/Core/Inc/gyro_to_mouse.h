/*
 * gyro_to_mouse.h
 *
 *  Created on: Apr 22, 2026
 *      Author: barbe
 */

#ifndef INC_GYRO_TO_MOUSE_H_
#define INC_GYRO_TO_MOUSE_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint16_t x;  // 0-4095
    uint16_t y;  // 0-4095
} MousePos_t;

void       GyroMouse_Init(void);
MousePos_t GyroMouse_Update(float mag_x, float mag_y, float mag_z, bool paused);
void       GyroMouse_GetSmoothedGyro(float raw_x, float raw_y, float *out_x, float *out_y);

#endif /* INC_GYRO_TO_MOUSE_H_ */
