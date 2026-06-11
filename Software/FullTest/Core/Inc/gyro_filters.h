/*
 * gyro_filters.h
 *
 *  Created on: Apr 22, 2026
 *      Author: barbe
 */

#ifndef INC_GYRO_FILTERS_H_
#define INC_GYRO_FILTERS_H_

#include <stdint.h>
#include <math.h>
#include "config.h"

#define LPF(raw, prev) \
    (gyro_lpf_alpha * (float)(raw) + (1.0f - gyro_lpf_alpha) * (prev))

/*#define HPF(raw, prev_lp, prev_hp) \
    ({ \
        float input = (float)(raw); \
        prev_lp = gyro_hpf_alpha * input + (1.0f - gyro_hpf_alpha) * prev_lp; \
        prev_hp = input - prev_lp; \
        prev_hp; \
    })

#define CF_GYRO_ACCEL(gyro_rate_dps, accel_angle_deg, prev_angle, dt) \
    ({ \
        float gyro_integral = (prev_angle) + (gyro_rate_dps) * (dt); \
        gyro_complementary_alpha * gyro_integral + (1.0f - gyro_complementary_alpha) * (accel_angle_deg); \
    })*/

#endif /* INC_GYRO_FILTERS_H_ */
