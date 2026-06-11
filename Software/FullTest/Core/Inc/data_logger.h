/*
 * data_logger.h
 *
 *  Created on: April 20, 2026
 *      Author: barbe
 */

#ifndef INC_DATA_LOGGER_H_
#define INC_DATA_LOGGER_H_

#include <stdint.h>
#include <stdbool.h>
#include "gesture_detector.h"
#include "config.h"
#include "ff.h"

// ============================================================================
// LIMITS
// ============================================================================

#define LOG_GESTURE_MAX_ENTRIES     10000
#define LOG_CONTINUOUS_MAX_ENTRIES  100000
#define LOG_ANALYZER_MAX_ENTRIES    50

#define LOG_GESTURE_FILENAME        "gestures.bin"
#define LOG_CONTINUOUS_FILENAME     "continuous.bin"
#define LOG_ANALYZER_FILENAME       "analyzer.bin"

#define CONFIG_FILENAME "config.txt"

// ============================================================================
// ENTRY TYPES
// ============================================================================

// What triggered this log entry
typedef enum
{
    LOG_EVENT_GESTURE_CONFIRMED  = 0,  // Full gesture detected
    LOG_EVENT_GESTURE_TIMEOUT    = 1,  // Entered DETECTING but timed out
    LOG_EVENT_NEAR_MISS_90       = 2,  // Magnitude hit 90% of threshold but didn't confirm
    LOG_EVENT_TOO_SENSITIVE      = 3,  // Magnitude exceeded threshold during idle (false positive risk)
} LogEventType_t;

// State change event for continuous log
typedef enum
{
    LOG_STATE_IDLE_TO_DETECTING  = 0,
    LOG_STATE_DETECTING_TO_DEBOUNCE = 1,
    LOG_STATE_DETECTING_TO_IDLE  = 2,  // timeout
    LOG_STATE_DEBOUNCE_TO_COOLDOWN = 3,
    LOG_STATE_COOLDOWN_TO_IDLE   = 4,
    LOG_STATE_PEAK_UPDATE        = 5,  // new peak magnitude during DETECTING
    LOG_STATE_NEAR_MISS_90       = 6,  // magnitude crossed 90% threshold
} LogContinuousEventType_t;

// ============================================================================
// CONFIG SNAPSHOT
// Captures config values at the time of the event so the analyzer
// knows what settings were in effect
// ============================================================================

typedef struct
{
    float    gesture_magnitude_threshold;
    uint32_t gesture_detection_window_ms;
    uint32_t gesture_cooldown_ms;
    uint32_t gesture_debounce_ms;
    float    gesture_peak_fraction;
    float    gyro_bias_x;
    float    gyro_bias_y;
    float    gyro_bias_z;
    float    gyro_lpf_alpha;
} ConfigSnapshot_t;  // 36 bytes

// ============================================================================
// GESTURE LOG ENTRY
// One entry per gesture event (confirmed or near-miss)
// ============================================================================

typedef struct
{
    uint32_t            timestamp_ms;       // when it happened
    LogEventType_t      event_type;         // confirmed, timeout, near-miss, too sensitive
    GestureType_t       gesture_result;     // which gesture (NONE if not confirmed)

    // Gyro at peak
    float               peak_gyro_x;
    float               peak_gyro_y;
    float               peak_gyro_z;
    float               peak_magnitude;

    // Raw sensor data at peak
    int16_t             accel_x;
    int16_t             accel_y;
    int16_t             accel_z;
    float               mag_x;
    float               mag_y;
    float               mag_z;

    // How long the gesture motion lasted
    uint32_t            duration_ms;

    // Config at time of event
    ConfigSnapshot_t    config_snapshot;

} GestureLogEntry_t;  // ~88 bytes

// ============================================================================
// CONTINUOUS LOG ENTRY
// Logged on state changes and interesting magnitude events
// ============================================================================

typedef struct
{
    uint32_t                    timestamp_ms;
    LogContinuousEventType_t    event_type;

    // Gyro at time of event
    float                       gyro_x;
    float                       gyro_y;
    float                       gyro_z;
    float                       magnitude;

    // Current peak during this detection window
    float                       peak_magnitude;

    // Threshold at time of event (for context)
    float                       threshold;

    // How close to threshold as a percentage (0.0 - 1.0+)
    float                       threshold_ratio;

} ContinuousLogEntry_t;  // ~44 bytes

typedef struct {
    uint32_t timestamp_ms;
    float    old_threshold;
    float    new_threshold;
    uint32_t old_window_ms;
    uint32_t new_window_ms;
    float    old_peak_fraction;
    float    new_peak_fraction;
    float    avg_peak;
    float    timeout_rate;
} AnalyzerChangeEntry_t;

// ============================================================================
// FILE HEADER
// Written at the start of each binary log file
// Contains circular buffer metadata
// ============================================================================

typedef struct
{
    uint32_t    magic;          // 0xDEADBEEF - confirms valid file
    uint32_t    version;        // file format version, increment if struct changes
    uint32_t    max_entries;    // max entries this file holds
    uint32_t    entry_size;     // sizeof the entry struct
    uint32_t    head;           // index of oldest entry (start reading here)
    uint32_t    tail;           // index of next write position
    uint32_t    count;          // total entries written (capped at max_entries)
    uint32_t    total_written;  // lifetime total including overwrites
} LogFileHeader_t;  // 32 bytes

// ============================================================================
// FUNCTIONS
// ============================================================================

// Init - call after SD mount
void DataLogger_Init(FATFS *fs, FIL *fil, char *path);

// Log a gesture event
void DataLogger_LogGesture(GestureLogEntry_t *entry);

// Log a continuous event
void DataLogger_LogContinuous(ContinuousLogEntry_t *entry);

// Flush RAM buffer to SD
void DataLogger_Flush(void);

// Periodic flush - call every loop, flushes every 30 seconds automatically
void DataLogger_Update(void);

// Clear functions
void DataLogger_ClearGestureLog(void);
void DataLogger_ClearContinuousLog(void);
void DataLogger_ClearAll(void);

// Read back last N gesture entries into a buffer for the analyzer
uint32_t DataLogger_GetLastGestureEntries(GestureLogEntry_t *buffer, uint32_t count);

// Read back last N continuous entries
uint32_t DataLogger_GetLastContinuousEntries(ContinuousLogEntry_t *buffer, uint32_t count);

// Print a gesture entry to serial for debugging
void DataLogger_PrintGestureEntry(GestureLogEntry_t *entry);

void DataLogger_SaveConfig(void);

void DataLogger_LoadConfig(void);

void DataLogger_LogAnalyzerChange(AnalyzerChangeEntry_t *entry);

uint32_t DataLogger_GetLastAnalyzerEntries(AnalyzerChangeEntry_t *buffer, uint32_t count);

void DataLogger_PrintAnalyzerEntry(AnalyzerChangeEntry_t *entry);

void DataLogger_ClearAnalyzerLog(void);

#endif /* INC_DATA_LOGGER_H_ */
