/*
 * gesture_detector.c
 *
 *  Created on: Apr 20, 2026
 *      Author: barbe
 */

#include "gesture_detector.h"
#include "data_logger.h"
#include "config.h"

// ============================================================================
// PRIVATE STATE
// ============================================================================

static GestureState_t g_state;
static uint32_t       g_state_start_ms;
static uint32_t       g_gesture_end_ms;
static uint32_t       g_detect_start_ms;

static float g_peak_magnitude;
static float g_peak_x;
static float g_peak_y;
static float g_peak_z;
static float g_last_logged_peak;

static float g_last_gyro_x;
static float g_last_gyro_y;
static float g_last_gyro_z;

static int16_t g_last_accel_x;
static int16_t g_last_accel_y;
static int16_t g_last_accel_z;

static float g_last_mag_x;
static float g_last_mag_y;
static float g_last_mag_z;

static bool g_near_miss_90_logged;
static bool g_too_sensitive_logged;

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

static float CalculateMagnitude(float x, float y, float z)
{
    return sqrtf(x * x + y * y + z * z);
}

static GestureType_t DetermineGestureFromPeak(void)
{
    float abs_x = fabsf(g_peak_x);
    float abs_y = fabsf(g_peak_y);
    float abs_z = fabsf(g_peak_z);

    if (abs_x > abs_y && abs_x > abs_z)
        return (g_peak_x > 0) ? GESTURE_FLICK_DOWN : GESTURE_FLICK_UP;
    else if (abs_y > abs_x && abs_y > abs_z)
        return (g_peak_y > 0) ? GESTURE_FLICK_LEFT : GESTURE_FLICK_RIGHT;
    else if (abs_z > abs_x && abs_z > abs_y)
        return (g_peak_z > 0) ? GESTURE_ROTATE_CW : GESTURE_ROTATE_CCW;

    return GESTURE_NONE;
}

static void LogGestureEvent(LogEventType_t event_type, GestureType_t gesture_result, uint32_t timestamp_ms)
{
    static GestureLogEntry_t entry;
    memset(&entry, 0, sizeof(GestureLogEntry_t));

    entry.timestamp_ms   = timestamp_ms;
    entry.event_type     = event_type;
    entry.gesture_result = gesture_result;
    entry.peak_gyro_x    = g_peak_x;
    entry.peak_gyro_y    = g_peak_y;
    entry.peak_gyro_z    = g_peak_z;
    entry.peak_magnitude = g_peak_magnitude;
    entry.duration_ms    = (g_detect_start_ms > 0)
                         ? (timestamp_ms - g_detect_start_ms)
                         : 0;
    entry.accel_x = g_last_accel_x;
    entry.accel_y = g_last_accel_y;
    entry.accel_z = g_last_accel_z;
    entry.mag_x   = g_last_mag_x;
    entry.mag_y   = g_last_mag_y;
    entry.mag_z   = g_last_mag_z;

    DataLogger_LogGesture(&entry);
}

static void LogContinuousEvent(LogContinuousEventType_t event_type,
                                float gyro_x, float gyro_y, float gyro_z,
                                float magnitude, float peak_magnitude,
                                uint32_t timestamp_ms)
{
    static ContinuousLogEntry_t entry;
    memset(&entry, 0, sizeof(ContinuousLogEntry_t));

    entry.timestamp_ms    = timestamp_ms;
    entry.event_type      = event_type;
    entry.gyro_x          = gyro_x;
    entry.gyro_y          = gyro_y;
    entry.gyro_z          = gyro_z;
    entry.magnitude       = magnitude;
    entry.peak_magnitude  = peak_magnitude;
    entry.threshold       = gesture_magnitude_threshold;
    entry.threshold_ratio = magnitude / gesture_magnitude_threshold;

    DataLogger_LogContinuous(&entry);
}

// ============================================================================
// PUBLIC FUNCTIONS
// ============================================================================

void GestureDetector_Init(void)
{
    g_state                = STATE_IDLE;
    g_state_start_ms       = 0;
    g_gesture_end_ms       = 0;
    g_detect_start_ms      = 0;
    g_peak_magnitude       = 0.0f;
    g_peak_x               = 0.0f;
    g_peak_y               = 0.0f;
    g_peak_z               = 0.0f;
    g_last_logged_peak     = 0.0f;
    g_last_gyro_x          = 0.0f;
    g_last_gyro_y          = 0.0f;
    g_last_gyro_z          = 0.0f;
    g_near_miss_90_logged  = false;
    g_too_sensitive_logged = false;
}

GestureType_t GestureDetector_Update(SensorData_t *data)
{
    float gyro_x      = data->gyro_x;
    float gyro_y      = data->gyro_y;
    float gyro_z      = data->gyro_z;
    uint32_t timestamp_ms = data->timestamp_ms;

    g_last_accel_x = data->accel_x;
    g_last_accel_y = data->accel_y;
    g_last_accel_z = data->accel_z;
    g_last_mag_x   = data->mag_x;
    g_last_mag_y   = data->mag_y;
    g_last_mag_z   = data->mag_z;
    g_last_gyro_x  = gyro_x;
    g_last_gyro_y  = gyro_y;
    g_last_gyro_z  = gyro_z;

    GestureType_t result = GESTURE_NONE;
    float magnitude = CalculateMagnitude(gyro_x, gyro_y, gyro_z);
    uint32_t elapsed_ms;

    switch (g_state)
    {
        case STATE_IDLE:
        {
            if (magnitude > gesture_magnitude_threshold)
            {
                if (!g_too_sensitive_logged)
                {
                    LogContinuousEvent(LOG_STATE_NEAR_MISS_90,
                                       gyro_x, gyro_y, gyro_z,
                                       magnitude, magnitude, timestamp_ms);
                    g_too_sensitive_logged = true;
                }

                g_state           = STATE_DETECTING;
                g_state_start_ms  = timestamp_ms;
                g_detect_start_ms = timestamp_ms;
                g_peak_magnitude  = magnitude;
                g_peak_x          = gyro_x;
                g_peak_y          = gyro_y;
                g_peak_z          = gyro_z;
                g_last_logged_peak     = 0.0f;
                g_near_miss_90_logged  = false;

                LogContinuousEvent(LOG_STATE_IDLE_TO_DETECTING,
                                   gyro_x, gyro_y, gyro_z,
                                   magnitude, magnitude, timestamp_ms);
            }
            else
            {
                g_too_sensitive_logged = false;

                if (magnitude > gesture_magnitude_threshold * 0.9f)
                {
                    if (!g_near_miss_90_logged)
                    {
                        LogContinuousEvent(LOG_STATE_NEAR_MISS_90,
                                           gyro_x, gyro_y, gyro_z,
                                           magnitude, magnitude, timestamp_ms);
                        g_peak_magnitude = magnitude;
                        g_peak_x = gyro_x;
                        g_peak_y = gyro_y;
                        g_peak_z = gyro_z;
                        LogGestureEvent(LOG_EVENT_NEAR_MISS_90, GESTURE_NONE, timestamp_ms);
                        g_near_miss_90_logged = true;
                    }
                }
                else
                {
                    g_near_miss_90_logged = false;
                }
            }
            break;
        }

        case STATE_DETECTING:
        {
            if (magnitude > g_peak_magnitude)
            {
                g_peak_magnitude = magnitude;
                g_peak_x = gyro_x;
                g_peak_y = gyro_y;
                g_peak_z = gyro_z;

                // Only log peak update if magnitude increased by 20% over last logged peak
                if (magnitude > g_last_logged_peak * 1.2f)
                {
                    LogContinuousEvent(LOG_STATE_PEAK_UPDATE,
                                       gyro_x, gyro_y, gyro_z,
                                       magnitude, g_peak_magnitude, timestamp_ms);
                    g_last_logged_peak = magnitude;
                }
            }

            if (magnitude < (gesture_magnitude_threshold * gesture_peak_fraction))
            {
                if (g_peak_magnitude >= gesture_magnitude_threshold)
                    result = DetermineGestureFromPeak();

                g_gesture_end_ms = timestamp_ms;
                g_state = STATE_DEBOUNCE;

                LogContinuousEvent(LOG_STATE_DETECTING_TO_DEBOUNCE,
                                   gyro_x, gyro_y, gyro_z,
                                   magnitude, g_peak_magnitude, timestamp_ms);
                break;
            }

            elapsed_ms = timestamp_ms - g_state_start_ms;
            if (elapsed_ms > gesture_detection_window_ms)
            {
                LogGestureEvent(LOG_EVENT_GESTURE_TIMEOUT, GESTURE_NONE, timestamp_ms);
                LogContinuousEvent(LOG_STATE_DETECTING_TO_IDLE,
                                   gyro_x, gyro_y, gyro_z,
                                   magnitude, g_peak_magnitude, timestamp_ms);
                g_state = STATE_IDLE;
            }
            break;
        }

        case STATE_DEBOUNCE:
        {
            elapsed_ms = timestamp_ms - g_gesture_end_ms;
            if (elapsed_ms >= gesture_debounce_ms)
            {
                g_state = STATE_COOLDOWN;
                LogContinuousEvent(LOG_STATE_DEBOUNCE_TO_COOLDOWN,
                                   gyro_x, gyro_y, gyro_z,
                                   magnitude, g_peak_magnitude, timestamp_ms);
            }
            break;
        }

        case STATE_COOLDOWN:
        {
            elapsed_ms = timestamp_ms - g_gesture_end_ms;
            if (elapsed_ms >= gesture_cooldown_ms)
            {
                g_state = STATE_IDLE;
                g_too_sensitive_logged = false;
                g_near_miss_90_logged  = false;
                LogContinuousEvent(LOG_STATE_COOLDOWN_TO_IDLE,
                                   gyro_x, gyro_y, gyro_z,
                                   magnitude, g_peak_magnitude, timestamp_ms);
            }
            break;
        }
    }

    return result;
}

GestureState_t GestureDetector_GetState(void)  { return g_state; }
float GestureDetector_GetPeakX(void)           { return g_peak_x; }
float GestureDetector_GetPeakY(void)           { return g_peak_y; }
float GestureDetector_GetPeakZ(void)           { return g_peak_z; }
float GestureDetector_GetPeakMagnitude(void)   { return g_peak_magnitude; }

const char* GestureDetector_Gesturetostring(GestureType_t gesture)
{
    switch (gesture)
    {
        case GESTURE_NONE:        return "NONE";
        case GESTURE_FLICK_LEFT:  return "FLICK_LEFT";
        case GESTURE_FLICK_RIGHT: return "FLICK_RIGHT";
        case GESTURE_FLICK_UP:    return "FLICK_UP";
        case GESTURE_FLICK_DOWN:  return "FLICK_DOWN";
        case GESTURE_ROTATE_CW:   return "ROTATE_CW";
        case GESTURE_ROTATE_CCW:  return "ROTATE_CCW";
        default:                  return "UNKNOWN";
    }
}

const char* GestureDetector_StateToString(GestureState_t state)
{
    switch (state)
    {
        case STATE_IDLE:      return "IDLE";
        case STATE_DETECTING: return "DETECTING";
        case STATE_DEBOUNCE:  return "DEBOUNCE";
        case STATE_COOLDOWN:  return "COOLDOWN";
        default:              return "UNKNOWN";
    }
}
