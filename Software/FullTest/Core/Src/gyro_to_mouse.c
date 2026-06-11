#include "gyro_to_mouse.h"
#include <math.h>

#define MAG_X_MIN  -50.0f
#define MAG_X_MAX   50.0f
#define MAG_Y_MIN  -50.0f
#define MAG_Y_MAX   50.0f

// Stored offset - the mag reading when we resumed
static float offset_x = 0.0f;
static float offset_y = 0.0f;

// Last frozen position (0-4095)
static uint16_t frozen_x = 2048;
static uint16_t frozen_y = 2048;

// Track previous pause state to detect rising edge (unpause)
static bool was_paused = false;

void GyroMouse_Init(void)
{
    offset_x  = 0.0f;
    offset_y  = 0.0f;
    frozen_x  = 2048;
    frozen_y  = 2048;
    was_paused = false;
}

static float NormalizeMag(float val, float min, float max)
{
    float norm = (val - (min + max) / 2.0f) / ((max - min) / 2.0f);
    if (norm >  1.0f) norm =  1.0f;
    if (norm < -1.0f) norm = -1.0f;
    return norm;
}

MousePos_t GyroMouse_Update(float mag_x, float mag_y, float mag_z, bool paused)
{
    MousePos_t pos;

    if (paused)
    {
        // Freeze - return last known position
        pos.x = frozen_x;
        pos.y = frozen_y;
        was_paused = true;
        return pos;
    }

    if (was_paused)
    {
        // Just unpaused - capture current mag as new offset
        // so position doesn't jump when released
        offset_x = NormalizeMag(mag_x, MAG_X_MIN, MAG_X_MAX);
        offset_y = NormalizeMag(mag_y, MAG_Y_MIN, MAG_Y_MAX);

        // Convert frozen position back to normalized -1 to 1
        // so we know where to continue from
        float frozen_nx = ((float)frozen_x / 4095.0f) * 2.0f - 1.0f;
        float frozen_ny = ((float)frozen_y / 4095.0f) * 2.0f - 1.0f;

        // Adjust offset so current mag maps to frozen position
        offset_x = NormalizeMag(mag_x, MAG_X_MIN, MAG_X_MAX) - frozen_nx;
        offset_y = NormalizeMag(mag_y, MAG_Y_MIN, MAG_Y_MAX) - frozen_ny;

        was_paused = false;
    }

    // Normal update - apply offset
    float nx = NormalizeMag(mag_x, MAG_X_MIN, MAG_X_MAX) - offset_x;
    float ny = NormalizeMag(mag_y, MAG_Y_MIN, MAG_Y_MAX) - offset_y;

    // Clamp
    if (nx >  1.0f) nx =  1.0f;
    if (nx < -1.0f) nx = -1.0f;
    if (ny >  1.0f) ny =  1.0f;
    if (ny < -1.0f) ny = -1.0f;

    // Map to 0-4095
    pos.x = (uint16_t)((nx + 1.0f) / 2.0f * 4095.0f);
    pos.y = (uint16_t)((ny + 1.0f) / 2.0f * 4095.0f);

    // Remember last position
    frozen_x = pos.x;
    frozen_y = pos.y;

    return pos;
}
