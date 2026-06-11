/*
 * icm20948.h
 *
 *  Created on: Mar 17, 2026
 *      Author: barbe
 */

#ifndef INC_ICM20948_H_
#define INC_ICM20948_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

// I2C Address (default 0x69, alternate 0x68 if SDO/ADR is connected to VIN)
#define ICM20948_ADDR_DEFAULT    0x69
#define ICM20948_ADDR_ALTERNATE  0x68

// Register Map
#define ICM20948_REG_WHO_AM_I    0x00
#define ICM20948_REG_PWR_MGMT_1  0x06
#define ICM20948_REG_PWR_MGMT_2  0x07
#define ICM20948_REG_ACCEL_XOUT_H 0x2D
#define ICM20948_REG_GYRO_XOUT_H 0x33

// Expected WHO_AM_I value
#define ICM20948_WHO_AM_I_VAL    0xEA

// Configuration values
#define ICM20948_PWR_SLEEP        0x40
#define ICM20948_PWR_CYCLE        0x20
#define ICM20948_PWR_STANDBY      0x10
#define ICM20948_PWR_RESET        0x80
#define ICM20948_PWR_WAKE         0x01

// Magnetometer (AK09916) registers
#define AK09916_I2C_ADDR              0x0C  // Magnetometer I2C address (7-bit)
#define AK09916_REG_WIA2               0x01  // WHO_AM_I register
#define AK09916_WIA2_VALUE             0x09  // Expected WHO_AM_I value
#define AK09916_REG_ST1                 0x10  // Status 1 register
#define AK09916_REG_HXL                 0x11  // Measurement data start (X LSB)
#define AK09916_REG_HXH                 0x12  // X MSB
#define AK09916_REG_HYL                 0x13  // Y LSB
#define AK09916_REG_HYH                 0x14  // Y MSB
#define AK09916_REG_HZL                 0x15  // Z LSB
#define AK09916_REG_HZH                 0x16  // Z MSB
#define AK09916_REG_ST2                 0x18  // Status 2 register
#define AK09916_REG_CNTL2               0x31  // Control register 2 (mode)
#define AK09916_REG_CNTL3               0x32  // Control register 3 (reset)

// Magnetometer modes
#define AK09916_MODE_POWER_DOWN        0x00
#define AK09916_MODE_SINGLE             0x01
#define AK09916_MODE_CONTINUOUS_10HZ    0x02
#define AK09916_MODE_CONTINUOUS_20HZ    0x04
#define AK09916_MODE_CONTINUOUS_50HZ    0x06
#define AK09916_MODE_CONTINUOUS_100HZ   0x08

// ICM-20948 registers for bypass mode
#define ICM20948_REG_INT_PIN_CFG        0x0F
#define ICM20948_REG_USER_CTRL           0x03
#define ICM20948_BIT_BYPASS_EN           0x02
#define ICM20948_BIT_I2C_MST_EN          0x20

// Sensor data structure
typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
    float temperature;
} ICM20948_Data_t;

// Magnetometer data structure
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
    float x_ut;      // Microteslas
    float y_ut;
    float z_ut;
} MagData_t;

// Public functions
HAL_StatusTypeDef ICM20948_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef ICM20948_ReadAccel(I2C_HandleTypeDef *hi2c, int16_t *ax, int16_t *ay, int16_t *az);
HAL_StatusTypeDef ICM20948_ReadGyro(I2C_HandleTypeDef *hi2c, int16_t *gx, int16_t *gy, int16_t *gz);
HAL_StatusTypeDef ICM20948_ReadAll(I2C_HandleTypeDef *hi2c, ICM20948_Data_t *data);
HAL_StatusTypeDef ICM20948_WhoAmI(I2C_HandleTypeDef *hi2c, uint8_t *id);

HAL_StatusTypeDef ICM20948_InitMagnetometer(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef ICM20948_ReadMagnetometer(I2C_HandleTypeDef *hi2c, MagData_t *mag);
HAL_StatusTypeDef ICM20948_ReadMagnetometerRaw(I2C_HandleTypeDef *hi2c, int16_t *mx, int16_t *my, int16_t *mz);
void ICM20948_ConvertMagToMicrotesla(MagData_t *mag);

void Debug_Magnetometer(I2C_HandleTypeDef *hi2c);

#ifdef __cplusplus
}
#endif


#endif
