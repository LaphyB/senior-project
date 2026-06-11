/*
 * gyro_to_mouse.c
 *
 *  Created on: Apr 22, 2026
 *      Author: barbe
 */

#include "gyro_to_mouse.h"
#include "gyro_filters.h"
#include <math.h>

#define MAG_X_MIN  -50.0f
#define MAG_X_MAX   50.0f
#define MAG_Y_MIN  -50.0f
#define MAG_Y_MAX   50.0f

static float    offset_x   = 0.0f;
static float    offset_y   = 0.0f;
static uint16_t frozen_x   = 2048;
static uint16_t frozen_y   = 2048;
static bool     was_paused = false;

// Mag smoothing
static float smooth_mag_x = 0.0f;
static float smooth_mag_y = 0.0f;
static float smooth_mag_z = 0.0f;
static bool  mag_first    = true;

// Gyro smoothing
static float smooth_gyro_x = 0.0f;
static float smooth_gyro_y = 0.0f;
static bool  gyro_first    = true;

static float NormalizeMag(float val, float min, float max)
{
    float norm = (val - (min + max) / 2.0f) / ((max - min) / 2.0f);
    if (norm >  1.0f) norm =  1.0f;
    if (norm < -1.0f) norm = -1.0f;
    return norm;
}

void GyroMouse_Init(void)
{
    offset_x     = 0.0f;
    offset_y     = 0.0f;
    frozen_x     = 2048;
    frozen_y     = 2048;
    was_paused   = false;
    smooth_mag_x = 0.0f;
    smooth_mag_y = 0.0f;
    smooth_mag_z = 0.0f;
    mag_first    = true;
    smooth_gyro_x = 0.0f;
    smooth_gyro_y = 0.0f;
    gyro_first   = true;
}

MousePos_t GyroMouse_Update(float mag_x, float mag_y, float mag_z, bool paused)
{
    MousePos_t pos;

    // Apply LPF to magnetometer
    if (mag_first)
    {
        smooth_mag_x = mag_x;
        smooth_mag_y = mag_y;
        smooth_mag_z = mag_z;
        mag_first = false;
    }
    else
    {
        smooth_mag_x = LPF(mag_x, smooth_mag_x);
        smooth_mag_y = LPF(mag_y, smooth_mag_y);
        smooth_mag_z = LPF(mag_z, smooth_mag_z);
    }

    if (paused)
    {
        pos.x = frozen_x;
        pos.y = frozen_y;
        was_paused = true;
        return pos;
    }

    if (was_paused)
    {
        float frozen_nx = ((float)frozen_x / 4095.0f) * 2.0f - 1.0f;
        float frozen_ny = ((float)frozen_y / 4095.0f) * 2.0f - 1.0f;

        offset_x = NormalizeMag(smooth_mag_x, MAG_X_MIN, MAG_X_MAX) - frozen_nx;
        offset_y = NormalizeMag(smooth_mag_y, MAG_Y_MIN, MAG_Y_MAX) - frozen_ny;

        was_paused = false;
    }

    float nx = NormalizeMag(smooth_mag_x, MAG_X_MIN, MAG_X_MAX) - offset_x;
    float ny = NormalizeMag(smooth_mag_y, MAG_Y_MIN, MAG_Y_MAX) - offset_y;

    if (nx >  1.0f) nx =  1.0f;
    if (nx < -1.0f) nx = -1.0f;
    if (ny >  1.0f) ny =  1.0f;
    if (ny < -1.0f) ny = -1.0f;

    pos.x = (uint16_t)((nx + 1.0f) / 2.0f * 4095.0f);
    pos.y = (uint16_t)((ny + 1.0f) / 2.0f * 4095.0f);

    frozen_x = pos.x;
    frozen_y = pos.y;

    return pos;
}

void GyroMouse_GetSmoothedGyro(float raw_x, float raw_y, float *out_x, float *out_y)
{
    if (gyro_first)
    {
        smooth_gyro_x = raw_x;
        smooth_gyro_y = raw_y;
        gyro_first = false;
    }
    else
    {
        smooth_gyro_x = LPF(raw_x, smooth_gyro_x);
        smooth_gyro_y = LPF(raw_y, smooth_gyro_y);
    }

    *out_x = smooth_gyro_x;
    *out_y = smooth_gyro_y;
}
