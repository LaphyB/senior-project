/*
 * config.h
 *
 *  Created on: April 20, 2026
 *      Author: barbe
 */

#ifndef SRC_CONFIGS_H_
#define SRC_CONFIGS_H_

#include <stdint.h>

// ============================================================================
// SYSTEM
// ============================================================================

#define HAL_DELAY_LENGTH 1

// ============================================================================
// GYRO BIAS
// ============================================================================

#define DEFAULT_GYRO_BIAS_X              -170.0f
#define DEFAULT_GYRO_BIAS_Y                 8.0f
#define DEFAULT_GYRO_BIAS_Z              2910.0f	// TODO: Z axis broken

// ============================================================================
// GYRO FILTER CONFIGURATION
// ============================================================================

#define DEFAULT_GYRO_LPF_ALPHA           0.1f
#define DEFAULT_GYRO_HPF_ALPHA           0.1f
#define DEFAULT_GYRO_COMPLIMENTARY_ALPHA 0.98f

// ============================================================================
// GESTURE DETECTOR CONFIGURATION
// ============================================================================

#define DEFAULT_GESTURE_MAGNITUDE_THRESHOLD  15000.0f
#define DEFAULT_GESTURE_DETECTION_WINDOW_MS  800
#define DEFAULT_GESTURE_COOLDOWN_MS          500
#define DEFAULT_GESTURE_DEBOUNCE_MS          100
#define DEFAULT_GESTURE_PEAK_FRACTION        0.2f

// ============================================================================
// GYRO TO MOUSE CONFIGURATION
// ============================================================================

#define DEFAULT_GYRO_SENSITIVITY_SCALE       131.0f
#define DEFAULT_GYRO_BASE_SENSITIVITY        1.0f
#define DEFAULT_GYRO_ACCEL_THRESHOLD_DPS     50.0f
#define DEFAULT_GYRO_ACCEL_MULTIPLIER        2.0f
#define DEFAULT_GYRO_MAX_DELTA               50
#define DEFAULT_GYRO_UPDATE_RATE_HZ          100
#define DEFAULT_GYRO_UPDATE_INTERVAL_MS      (1000 / DEFAULT_GYRO_UPDATE_RATE_HZ)

// ============================================================================
// RUNTIME GLOBALS
// ============================================================================

extern float    gyro_bias_x;
extern float    gyro_bias_y;
extern float    gyro_bias_z;

extern float    gyro_lpf_alpha;
extern float    gyro_hpf_alpha;
extern float    gyro_complementary_alpha;

extern float    gesture_magnitude_threshold;
extern uint32_t gesture_detection_window_ms;
extern uint32_t gesture_cooldown_ms;
extern uint32_t gesture_debounce_ms;
extern float    gesture_peak_fraction;

extern float    gyro_sensitivity_scale;
extern float    gyro_base_sensitivity;
extern float    gyro_accel_threshold_dps;
extern float    gyro_accel_multiplier;
extern int32_t  gyro_max_delta;
extern int32_t  gyro_update_rate_hz;
extern int32_t  gyro_update_interval_ms;

void Config_Init(void);

#endif /* SRC_CONFIGS_H_ */
