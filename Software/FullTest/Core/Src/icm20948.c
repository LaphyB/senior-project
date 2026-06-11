/*
 * icm20948.c
 *
 *  Created on: Mar 17, 2026
 *      Author: barbe
 */

#include "icm20948.h"

// Private helper functions
static HAL_StatusTypeDef icm20948_write_reg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t value) {
    return HAL_I2C_Mem_Write(hi2c, ICM20948_ADDR_DEFAULT << 1, reg,
                             I2C_MEMADD_SIZE_8BIT, &value, 1, HAL_MAX_DELAY);
}

static HAL_StatusTypeDef icm20948_read_regs(I2C_HandleTypeDef *hi2c, uint8_t reg,
                                            uint8_t *data, uint16_t len) {
    return HAL_I2C_Mem_Read(hi2c, ICM20948_ADDR_DEFAULT << 1, reg,
                           I2C_MEMADD_SIZE_8BIT, data, len, HAL_MAX_DELAY);
}

/**
 * @brief Read WHO_AM_I register to verify connection
 */
HAL_StatusTypeDef ICM20948_WhoAmI(I2C_HandleTypeDef *hi2c, uint8_t *id) {
    return icm20948_read_regs(hi2c, ICM20948_REG_WHO_AM_I, id, 1);
}

/**
 * @brief Initialize the ICM-20948 sensor
 */
HAL_StatusTypeDef ICM20948_Init(I2C_HandleTypeDef *hi2c) {
    uint8_t who_am_i = 0;
    HAL_StatusTypeDef status;

    // Check if device is connected
    status = ICM20948_WhoAmI(hi2c, &who_am_i);
    if (status != HAL_OK || who_am_i != ICM20948_WHO_AM_I_VAL) {
        return HAL_ERROR;
    }

    // Reset device
    status = icm20948_write_reg(hi2c, ICM20948_REG_PWR_MGMT_1, ICM20948_PWR_RESET);
    if (status != HAL_OK) return status;
    HAL_Delay(100);

    // Wake up device (clear sleep)
    status = icm20948_write_reg(hi2c, ICM20948_REG_PWR_MGMT_1, ICM20948_PWR_WAKE);
    if (status != HAL_OK) return status;
    HAL_Delay(10);

    // Auto-select clock source
    status = icm20948_write_reg(hi2c, ICM20948_REG_PWR_MGMT_1, 0x01);
    if (status != HAL_OK) return status;

    return HAL_OK;
}

/**
 * @brief Read accelerometer data
 */
HAL_StatusTypeDef ICM20948_ReadAccel(I2C_HandleTypeDef *hi2c,
                                     int16_t *ax, int16_t *ay, int16_t *az) {
    uint8_t data[6];
    HAL_StatusTypeDef status;

    status = icm20948_read_regs(hi2c, ICM20948_REG_ACCEL_XOUT_H, data, 6);
    if (status != HAL_OK) return status;

    *ax = (int16_t)((data[0] << 8) | data[1]);
    *ay = (int16_t)((data[2] << 8) | data[3]);
    *az = (int16_t)((data[4] << 8) | data[5]);

    return HAL_OK;
}

/**
 * @brief Read gyroscope data
 */
HAL_StatusTypeDef ICM20948_ReadGyro(I2C_HandleTypeDef *hi2c,
                                    int16_t *gx, int16_t *gy, int16_t *gz) {
    uint8_t data[6];
    HAL_StatusTypeDef status;

    status = icm20948_read_regs(hi2c, ICM20948_REG_GYRO_XOUT_H, data, 6);
    if (status != HAL_OK) return status;

    *gx = (int16_t)((data[0] << 8) | data[1]);
    *gy = (int16_t)((data[2] << 8) | data[3]);
    *gz = (int16_t)((data[4] << 8) | data[5]);

    return HAL_OK;
}

/**
 * @brief Read all sensor data (accel, gyro, temp)
 */
HAL_StatusTypeDef ICM20948_ReadAll(I2C_HandleTypeDef *hi2c, ICM20948_Data_t *data) {
    uint8_t buffer[14];
    HAL_StatusTypeDef status;

    // Read all registers from ACCEL_XOUT_H to GYRO_ZOUT_H (14 bytes)
    status = icm20948_read_regs(hi2c, ICM20948_REG_ACCEL_XOUT_H, buffer, 14);
    if (status != HAL_OK) return status;

    // Parse accelerometer data (first 6 bytes)
    data->accel_x = (int16_t)((buffer[0] << 8) | buffer[1]);
    data->accel_y = (int16_t)((buffer[2] << 8) | buffer[3]);
    data->accel_z = (int16_t)((buffer[4] << 8) | buffer[5]);

    // Temperature data (next 2 bytes, ignore for now)
    // data->temperature = (int16_t)((buffer[6] << 8) | buffer[7]) / 333.87f + 21.0f;

    // Parse gyroscope data (last 6 bytes)
    data->gyro_x = (int16_t)((buffer[8] << 8) | buffer[9]);
    data->gyro_y = (int16_t)((buffer[10] << 8) | buffer[11]);
    data->gyro_z = (int16_t)((buffer[12] << 8) | buffer[13]);

    return HAL_OK;
}

/**
 * @brief Enable bypass mode to access magnetometer directly
 */
static HAL_StatusTypeDef ICM20948_EnableBypassMode(I2C_HandleTypeDef *hi2c) {
    uint8_t user_ctrl, int_pin_cfg;
    HAL_StatusTypeDef status;

    printf("Mag: Enabling bypass mode...\r\n");

    // First, disable I2C master mode if enabled
    status = icm20948_read_regs(hi2c, ICM20948_REG_USER_CTRL, &user_ctrl, 1);
    if (status != HAL_OK) {
        printf("Mag: Failed to read USER_CTRL\r\n");
        return status;
    }

    user_ctrl &= ~ICM20948_BIT_I2C_MST_EN;  // Clear I2C_MST_EN bit
    status = icm20948_write_reg(hi2c, ICM20948_REG_USER_CTRL, user_ctrl);
    if (status != HAL_OK) {
        printf("Mag: Failed to disable I2C master\r\n");
        return status;
    }

    // Enable bypass mode
    status = icm20948_read_regs(hi2c, ICM20948_REG_INT_PIN_CFG, &int_pin_cfg, 1);
    if (status != HAL_OK) {
        printf("Mag: Failed to read INT_PIN_CFG\r\n");
        return status;
    }

    int_pin_cfg |= ICM20948_BIT_BYPASS_EN;  // Set bypass enable bit
    status = icm20948_write_reg(hi2c, ICM20948_REG_INT_PIN_CFG, int_pin_cfg);
    if (status != HAL_OK) {
        printf("Mag: Failed to enable bypass mode\r\n");
        return status;
    }

    HAL_Delay(10);
    printf("Mag: Bypass mode enabled\r\n");
    return HAL_OK;
}

/**
 * @brief Initialize AK09916 magnetometer
 */
HAL_StatusTypeDef ICM20948_InitMagnetometer(I2C_HandleTypeDef *hi2c) {
    uint8_t who_am_i;
    uint8_t mode;
    HAL_StatusTypeDef status;

    printf("Initializing magnetometer...\r\n");

    // First enable bypass mode
    status = ICM20948_EnableBypassMode(hi2c);
    if (status != HAL_OK) {
        printf("Mag: Failed to enable bypass mode\r\n");
        return status;
    }

    // Check magnetometer WHO_AM_I
    printf("Mag: Checking WHO_AM_I...\r\n");
    status = HAL_I2C_Mem_Read(hi2c, AK09916_I2C_ADDR << 1,
                              AK09916_REG_WIA2, I2C_MEMADD_SIZE_8BIT,
                              &who_am_i, 1, HAL_MAX_DELAY);

    if (status != HAL_OK) {
        printf("Mag: Failed to read WHO_AM_I (status: %d)\r\n", status);
        return HAL_ERROR;
    }

    printf("Mag: WHO_AM_I = 0x%02X (expected 0x09)\r\n", who_am_i);

    if (who_am_i != AK09916_WIA2_VALUE) {
        printf("Mag: Wrong WHO_AM_I value!\r\n");
        return HAL_ERROR;
    }

    // Soft reset magnetometer
    printf("Mag: Resetting...\r\n");
    uint8_t reset_cmd = 0x01;
    status = HAL_I2C_Mem_Write(hi2c, AK09916_I2C_ADDR << 1,
                               AK09916_REG_CNTL3, I2C_MEMADD_SIZE_8BIT,
                               &reset_cmd, 1, HAL_MAX_DELAY);
    if (status != HAL_OK) {
        printf("Mag: Reset failed\r\n");
        return status;
    }
    HAL_Delay(10);

    // Set to continuous mode at 100Hz
    printf("Mag: Setting continuous mode 100Hz...\r\n");
    mode = AK09916_MODE_CONTINUOUS_100HZ;
    status = HAL_I2C_Mem_Write(hi2c, AK09916_I2C_ADDR << 1,
                               AK09916_REG_CNTL2, I2C_MEMADD_SIZE_8BIT,
                               &mode, 1, HAL_MAX_DELAY);

    if (status != HAL_OK) {
        printf("Mag: Failed to set mode\r\n");
        return status;
    }

    printf("Magnetometer initialized successfully!\r\n");
    return HAL_OK;
}

/**
 * @brief Read raw magnetometer data (int16_t values)
 */
HAL_StatusTypeDef ICM20948_ReadMagnetometerRaw(I2C_HandleTypeDef *hi2c,
                                               int16_t *mx, int16_t *my, int16_t *mz) {
    uint8_t data[6];
    uint8_t st1, st2;
    HAL_StatusTypeDef status;

    // Check if data is ready
    status = HAL_I2C_Mem_Read(hi2c, AK09916_I2C_ADDR << 1,
                              AK09916_REG_ST1, I2C_MEMADD_SIZE_8BIT,
                              &st1, 1, HAL_MAX_DELAY);

    if (status != HAL_OK) {
        return status;
    }

    if (!(st1 & 0x01)) {
        return HAL_BUSY;  // Data not ready
    }

    // Read magnetometer data (6 bytes: X, Y, Z each LSB then MSB)
    status = HAL_I2C_Mem_Read(hi2c, AK09916_I2C_ADDR << 1,
                              AK09916_REG_HXL, I2C_MEMADD_SIZE_8BIT,
                              data, 6, HAL_MAX_DELAY);

    if (status == HAL_OK) {
        *mx = (int16_t)((data[1] << 8) | data[0]);  // X: HXL, HXH
        *my = (int16_t)((data[3] << 8) | data[2]);  // Y: HYL, HYH
        *mz = (int16_t)((data[5] << 8) | data[4]);  // Z: HZL, HZH

        // Read ST2 register to end the read sequence (required by datasheet)
        HAL_I2C_Mem_Read(hi2c, AK09916_I2C_ADDR << 1,
                         AK09916_REG_ST2, I2C_MEMADD_SIZE_8BIT,
                         &st2, 1, HAL_MAX_DELAY);
    }

    return status;
}

/**
 * @brief Read magnetometer data with conversion to microtesla
 */
HAL_StatusTypeDef ICM20948_ReadMagnetometer(I2C_HandleTypeDef *hi2c, MagData_t *mag) {
    HAL_StatusTypeDef status;

    status = ICM20948_ReadMagnetometerRaw(hi2c, &mag->x, &mag->y, &mag->z);

    if (status == HAL_OK) {
        // Convert to microteslas (0.15 µT per LSB)
        mag->x_ut = mag->x * 0.15f;
        mag->y_ut = mag->y * 0.15f;
        mag->z_ut = mag->z * 0.15f;
    }

    return status;
}

/**
 * @brief Convert raw magnetometer values to microtesla
 */
void ICM20948_ConvertMagToMicrotesla(MagData_t *mag) {
    mag->x_ut = mag->x * 0.15f;
    mag->y_ut = mag->y * 0.15f;
    mag->z_ut = mag->z * 0.15f;
}

/* INC_ICM20948_H_ */

/*// ===== I2C DEBUG SECTION =====
  printf("\r\n=== I2C Debug Start ===\r\n");

  // Check I2C state
  printf("I2C State: 0x%02lX\r\n", HAL_I2C_GetState(&hi2c1));
  printf("I2C Error: 0x%02lX\r\n", HAL_I2C_GetError(&hi2c1));

  // Try to scan I2C bus
  printf("Scanning I2C bus...\r\n");
  uint8_t found_devices = 0;

  for (uint8_t addr = 0x01; addr < 0x7F; addr++) {
      HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 3, 100);
      if (status == HAL_OK) {
          printf("Found device at address 0x%02X\r\n", addr);
          found_devices++;
      }
  }

  if (found_devices == 0) {
      printf("NO I2C DEVICES FOUND!\r\n");
      printf("Check wiring:\r\n");
      printf("  - VIN (ICM-20948) -> 3V3 on Nucleo\r\n");
      printf("  - GND -> GND\r\n");
      printf("  - SDA -> PB7\r\n");
      printf("  - SCL -> PB6\r\n");
      printf("  - Are pull-up resistors present? (Adafruit board has them)\r\n");
  }

  // Try specific ICM-20948 addresses
  printf("\r\nTrying ICM-20948 specifically:\r\n");

  // Try default address (0x69)
  uint8_t who_am_i = 0;
  HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c1,
                                             0x69 << 1,  // Default address
                                             0x00,       // WHO_AM_I register
                                             I2C_MEMADD_SIZE_8BIT,
                                             &who_am_i,
                                             1,
                                             1000);

  if (status == HAL_OK) {
      printf("Address 0x69 responded! WHO_AM_I = 0x%02X (expected 0xEA)\r\n", who_am_i);
  } else {
      printf("Address 0x69 failed with status: %d\r\n", status);

      // Try alternate address (0x68)
      status = HAL_I2C_Mem_Read(&hi2c1,
                                0x68 << 1,  // Alternate address
                                0x00,
                                I2C_MEMADD_SIZE_8BIT,
                                &who_am_i,
                                1,
                                1000);

      if (status == HAL_OK) {
          printf("Address 0x68 responded! WHO_AM_I = 0x%02X\r\n", who_am_i);
      } else {
          printf("Address 0x68 also failed with status: %d\r\n", status);
      }
  }

  // Show detailed error codes
  if (status != HAL_OK) {
      printf("\r\nDetailed I2C Error Analysis:\r\n");
      uint32_t error = HAL_I2C_GetError(&hi2c1);

      if (error & HAL_I2C_ERROR_BERR) printf("  - Bus Error\r\n");
      if (error & HAL_I2C_ERROR_ARLO) printf("  - Arbitration Lost\r\n");
      if (error & HAL_I2C_ERROR_AF) printf("  - Acknowledge Failure (NACK)\r\n");
      if (error & HAL_I2C_ERROR_OVR) printf("  - Overrun/Underrun\r\n");
      if (error & HAL_I2C_ERROR_DMA) printf("  - DMA Error\r\n");
      if (error & HAL_I2C_ERROR_TIMEOUT) printf("  - Timeout\r\n");
      if (error == HAL_I2C_ERROR_NONE) printf("  - No specific error (bus may be stuck)\r\n");
  }

  printf("=== I2C Debug End ===\r\n\r\n");

  // If no devices found, blink LED rapidly to indicate error
  if (found_devices == 0) {
      while(1) {
          HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
          HAL_Delay(100);  // Fast blink = I2C error
      }
  }

  // ===== END I2C DEBUG =====*/





/* USER CODE BEGIN 4 */
void Debug_Magnetometer(I2C_HandleTypeDef *hi2c) {
    printf("\r\n=== Magnetometer Debug ===\r\n");

    // 1. Check if we can talk to magnetometer at all
    printf("1. Checking magnetometer I2C address 0x0C... ");
    HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(hi2c, 0x0C << 1, 5, 100);
    if (status == HAL_OK) {
        printf("RESPONDING\r\n");
    } else {
        printf("NO RESPONSE (status: %d)\r\n", status);
    }

    // 2. Try to read WHO_AM_I
    printf("2. Reading WHO_AM_I... ");
    uint8_t who_am_i = 0;
    status = HAL_I2C_Mem_Read(hi2c, 0x0C << 1, 0x01, I2C_MEMADD_SIZE_8BIT, &who_am_i, 1, 100);
    if (status == HAL_OK) {
        printf("0x%02X (expected 0x09)\r\n", who_am_i);
    } else {
        printf("FAILED (status: %d)\r\n", status);
    }

    // 3. Check ST1 register
    printf("3. Reading ST1 register... ");
    uint8_t st1 = 0;
    status = HAL_I2C_Mem_Read(hi2c, 0x0C << 1, 0x10, I2C_MEMADD_SIZE_8BIT, &st1, 1, 100);
    if (status == HAL_OK) {
        printf("0x%02X (DRDY bit: %d)\r\n", st1, (st1 & 0x01) ? 1 : 0);
    } else {
        printf("FAILED (status: %d)\r\n", status);
    }

    // 4. Check CNTL2 register (mode setting)
    printf("4. Reading CNTL2 register... ");
    uint8_t cntl2 = 0;
    status = HAL_I2C_Mem_Read(hi2c, 0x0C << 1, 0x31, I2C_MEMADD_SIZE_8BIT, &cntl2, 1, 100);
    if (status == HAL_OK) {
        printf("0x%02X (mode: ", cntl2);
        switch(cntl2) {
            case 0x00: printf("Power-down"); break;
            case 0x01: printf("Single"); break;
            case 0x02: printf("10Hz continuous"); break;
            case 0x04: printf("20Hz continuous"); break;
            case 0x06: printf("50Hz continuous"); break;
            case 0x08: printf("100Hz continuous"); break;
            default: printf("Unknown");
        }
        printf(")\r\n");
    } else {
        printf("FAILED (status: %d)\r\n", status);
    }

    printf("=== Debug Complete ===\r\n\r\n");
}
/* USER CODE END 4 */
