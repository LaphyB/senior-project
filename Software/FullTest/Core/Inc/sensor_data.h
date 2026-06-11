/*
 * sensor_data.h
 *
 *  Created on: April 20, 2026
 *      Author: barbe
 */

#ifndef INC_SENSOR_DATA_H_
#define INC_SENSOR_DATA_H_

#include <stdint.h>
#include "buttons.h"

typedef struct
{
    // Timestamp when this data was captured (ms from HAL_GetTick)
    uint32_t timestamp_ms;

    // Button values
    ButtonStates_t buttons;

    // Joystick values (0-4095 from ADC)
    uint16_t js1_x;
    uint16_t js1_y;
    uint16_t js2_x;
    uint16_t js2_y;

    // Gyroscope data (raw from ICM20948)
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;

    // Accelerometer data (raw from ICM20948)
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;

    // Magnetometer data
    float mag_x;
    float mag_y;
    float mag_z;

} SensorData_t;

#endif /* INC_SENSOR_DATA_H_ */
