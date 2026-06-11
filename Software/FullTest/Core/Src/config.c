/*
 * config.c
 *
 *  Created on: April 20, 2026
 *      Author: barbe
 */

#include "config.h"

// Gyro bias
float    gyro_bias_x;
float    gyro_bias_y;
float    gyro_bias_z;

// Gyro filters
float    gyro_lpf_alpha;
float    gyro_hpf_alpha;
float    gyro_complementary_alpha;

// Gesture detection
float    gesture_magnitude_threshold;
uint32_t gesture_detection_window_ms;
uint32_t gesture_cooldown_ms;
uint32_t gesture_debounce_ms;
float    gesture_peak_fraction;

// Gyro to mouse
float    gyro_sensitivity_scale;
float    gyro_base_sensitivity;
float    gyro_accel_threshold_dps;
float    gyro_accel_multiplier;
int32_t  gyro_max_delta;
int32_t  gyro_update_rate_hz;
int32_t  gyro_update_interval_ms;

void Config_Init(void)
{
    gyro_bias_x                  = DEFAULT_GYRO_BIAS_X;
    gyro_bias_y                  = DEFAULT_GYRO_BIAS_Y;
    gyro_bias_z                  = DEFAULT_GYRO_BIAS_Z;

    gyro_lpf_alpha               = DEFAULT_GYRO_LPF_ALPHA;
    gyro_hpf_alpha               = DEFAULT_GYRO_HPF_ALPHA;
    gyro_complementary_alpha     = DEFAULT_GYRO_COMPLIMENTARY_ALPHA;

    gesture_magnitude_threshold  = DEFAULT_GESTURE_MAGNITUDE_THRESHOLD;
    gesture_detection_window_ms  = DEFAULT_GESTURE_DETECTION_WINDOW_MS;
    gesture_cooldown_ms          = DEFAULT_GESTURE_COOLDOWN_MS;
    gesture_debounce_ms          = DEFAULT_GESTURE_DEBOUNCE_MS;
    gesture_peak_fraction        = DEFAULT_GESTURE_PEAK_FRACTION;

    gyro_sensitivity_scale       = DEFAULT_GYRO_SENSITIVITY_SCALE;
    gyro_base_sensitivity        = DEFAULT_GYRO_BASE_SENSITIVITY;
    gyro_accel_threshold_dps     = DEFAULT_GYRO_ACCEL_THRESHOLD_DPS;
    gyro_accel_multiplier        = DEFAULT_GYRO_ACCEL_MULTIPLIER;
    gyro_max_delta               = DEFAULT_GYRO_MAX_DELTA;
    gyro_update_rate_hz          = DEFAULT_GYRO_UPDATE_RATE_HZ;
    gyro_update_interval_ms      = DEFAULT_GYRO_UPDATE_INTERVAL_MS;
}
