/*
 * data_logger.c
 *
 *  Created on: April 20, 2026
 *      Author: barbe
 */

#include "data_logger.h"
#include "gesture_detector.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// DEFINES
// ============================================================================

#define LOG_MAGIC              0xDEADBEEF
#define LOG_VERSION            1
#define GESTURE_BUFFER_SIZE    3
#define CONTINUOUS_BUFFER_SIZE 3
#define FLUSH_INTERVAL_MS      30000

// ============================================================================
// PRIVATE STATE
// ============================================================================

static FATFS  *s_fs   = NULL;
static FIL    *s_fil  = NULL;
static char   *s_path = NULL;

static GestureLogEntry_t    s_gesture_buffer[GESTURE_BUFFER_SIZE];
static uint32_t             s_gesture_buffer_count = 0;

static ContinuousLogEntry_t s_continuous_buffer[CONTINUOUS_BUFFER_SIZE];
static uint32_t             s_continuous_buffer_count = 0;

static uint32_t s_last_flush_ms = 0;

// Permanently open file handles
static FIL     s_gesture_file;
static FIL     s_continuous_file;
static uint8_t s_files_open = 0;

static LogFileHeader_t s_gesture_header;
static LogFileHeader_t s_continuous_header;

static FIL     s_analyzer_file;
static LogFileHeader_t s_analyzer_header;

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

static ConfigSnapshot_t MakeConfigSnapshot(void)
{
    ConfigSnapshot_t snap;
    snap.gesture_magnitude_threshold = gesture_magnitude_threshold;
    snap.gesture_detection_window_ms = gesture_detection_window_ms;
    snap.gesture_cooldown_ms         = gesture_cooldown_ms;
    snap.gesture_debounce_ms         = gesture_debounce_ms;
    snap.gesture_peak_fraction       = gesture_peak_fraction;
    snap.gyro_bias_x                 = gyro_bias_x;
    snap.gyro_bias_y                 = gyro_bias_y;
    snap.gyro_bias_z                 = gyro_bias_z;
    snap.gyro_lpf_alpha              = gyro_lpf_alpha;
    return snap;
}

static void InitLogFile(const char *filename, uint32_t max_entries, uint32_t entry_size)
{
    FIL fil;
    LogFileHeader_t header;
    FRESULT fres;
    UINT bw;

    char full_path[32];
    snprintf(full_path, sizeof(full_path), "%s%s", s_path, filename);

    fres = f_open(&fil, full_path, FA_READ | FA_WRITE);
    if (fres == FR_OK)
    {
        UINT br;
        fres = f_read(&fil, &header, sizeof(LogFileHeader_t), &br);

        if (fres == FR_OK &&
            br == sizeof(LogFileHeader_t) &&
            header.magic       == LOG_MAGIC &&
            header.version     == LOG_VERSION &&
            header.entry_size  == entry_size &&
            header.max_entries == max_entries)
        {
            printf("DataLogger: %s OK, %lu entries\r\n",
                   filename, (unsigned long)header.count);
            f_close(&fil);
            return;
        }

        printf("DataLogger: %s invalid header, recreating\r\n", filename);
        f_close(&fil);
        f_unlink(full_path);
    }

    fres = f_open(&fil, full_path, FA_CREATE_NEW | FA_WRITE);
    if (fres != FR_OK)
    {
        printf("DataLogger: %s create failed fres=%d\r\n", filename, fres);
        return;
    }

    memset(&header, 0, sizeof(LogFileHeader_t));
    header.magic         = LOG_MAGIC;
    header.version       = LOG_VERSION;
    header.max_entries   = max_entries;
    header.entry_size    = entry_size;
    header.head          = 0;
    header.tail          = 0;
    header.count         = 0;
    header.total_written = 0;

    f_write(&fil, &header, sizeof(LogFileHeader_t), &bw);
    f_sync(&fil);
    f_close(&fil);

    printf("DataLogger: %s created\r\n", filename);
}

static void WriteEntry(FIL *fil, LogFileHeader_t *header, const void *entry, uint32_t entry_size)
{
    UINT bw;

    // Seek directly to write position - no header read needed
    uint32_t offset = sizeof(LogFileHeader_t) + (header->tail * entry_size);
    if (f_lseek(fil, offset) != FR_OK) return;
    if (f_write(fil, entry, entry_size, &bw) != FR_OK) return;

    // Update cached header
    header->tail = (header->tail + 1) % header->max_entries;
    header->total_written++;
    if (header->count < header->max_entries)
        header->count++;
    else
        header->head = (header->head + 1) % header->max_entries;
}

static void FlushGestureBuffer(void)
{
    if (s_gesture_buffer_count == 0 || !s_files_open) return;

    for (uint32_t i = 0; i < s_gesture_buffer_count; i++)
        WriteEntry(&s_gesture_file, &s_gesture_header,
                   &s_gesture_buffer[i], sizeof(GestureLogEntry_t));

    // Write header once after all entries
    f_lseek(&s_gesture_file, 0);
    UINT bw;
    f_write(&s_gesture_file, &s_gesture_header, sizeof(LogFileHeader_t), &bw);
    f_sync(&s_gesture_file);

    s_gesture_buffer_count = 0;
}

static void FlushContinuousBuffer(void)
{
    if (s_continuous_buffer_count == 0 || !s_files_open) return;

    for (uint32_t i = 0; i < s_continuous_buffer_count; i++)
        WriteEntry(&s_continuous_file, &s_continuous_header,
                   &s_continuous_buffer[i], sizeof(ContinuousLogEntry_t));

    f_lseek(&s_continuous_file, 0);
    UINT bw;
    f_write(&s_continuous_file, &s_continuous_header, sizeof(LogFileHeader_t), &bw);
    f_sync(&s_continuous_file);

    s_continuous_buffer_count = 0;
}

// ============================================================================
// PUBLIC FUNCTIONS
// ============================================================================

void DataLogger_Init(FATFS *fs, FIL *fil, char *path)
{
    s_fs   = fs;
    s_fil  = fil;
    s_path = path;

    s_gesture_buffer_count    = 0;
    s_continuous_buffer_count = 0;
    s_last_flush_ms           = HAL_GetTick();
    s_files_open              = 0;

    UINT br;

    printf("DataLogger: initializing...\r\n");

    FRESULT fres = f_mount(fs, path, 1);
    if (fres != FR_OK)
    {
        printf("DataLogger: SD mount failed (%d)\r\n", fres);
        return;
    }

    InitLogFile(LOG_GESTURE_FILENAME,    LOG_GESTURE_MAX_ENTRIES,    sizeof(GestureLogEntry_t));
    InitLogFile(LOG_CONTINUOUS_FILENAME, LOG_CONTINUOUS_MAX_ENTRIES, sizeof(ContinuousLogEntry_t));
    InitLogFile(LOG_ANALYZER_FILENAME,   LOG_ANALYZER_MAX_ENTRIES,   sizeof(AnalyzerChangeEntry_t));

    char full_path[32];

    snprintf(full_path, sizeof(full_path), "%s%s", s_path, LOG_GESTURE_FILENAME);
    fres = f_open(&s_gesture_file, full_path, FA_READ | FA_WRITE);
    if (fres != FR_OK)
    {
        printf("DataLogger: failed to open gesture file (%d)\r\n", fres);
        return;
    }
    f_lseek(&s_gesture_file, 0);
    f_read(&s_gesture_file, &s_gesture_header, sizeof(LogFileHeader_t), &br);

    snprintf(full_path, sizeof(full_path), "%s%s", s_path, LOG_CONTINUOUS_FILENAME);
    fres = f_open(&s_continuous_file, full_path, FA_READ | FA_WRITE);
    if (fres != FR_OK)
    {
        printf("DataLogger: failed to open continuous file (%d)\r\n", fres);
        f_close(&s_gesture_file);
        return;
    }
    f_lseek(&s_continuous_file, 0);
    f_read(&s_continuous_file, &s_continuous_header, sizeof(LogFileHeader_t), &br);

    // Open analyzer file
    snprintf(full_path, sizeof(full_path), "%s%s", s_path, LOG_ANALYZER_FILENAME);
    fres = f_open(&s_analyzer_file, full_path, FA_READ | FA_WRITE);
    if (fres != FR_OK)
    {
        printf("DataLogger: failed to open analyzer file (%d)\r\n", fres);
        f_close(&s_gesture_file);
        f_close(&s_continuous_file);
        return;
    }
    f_lseek(&s_analyzer_file, 0);
    f_read(&s_analyzer_file, &s_analyzer_header, sizeof(LogFileHeader_t), &br);

    s_files_open = 1;
    printf("DataLogger: ready\r\n");
}

void DataLogger_LogGesture(GestureLogEntry_t *entry)
{
    entry->config_snapshot = MakeConfigSnapshot();

    if (s_gesture_buffer_count < GESTURE_BUFFER_SIZE)
        s_gesture_buffer[s_gesture_buffer_count++] = *entry;
    else
    {
        FlushGestureBuffer();
        s_gesture_buffer[s_gesture_buffer_count++] = *entry;
    }
}

void DataLogger_LogContinuous(ContinuousLogEntry_t *entry)
{
    if (s_continuous_buffer_count < CONTINUOUS_BUFFER_SIZE)
        s_continuous_buffer[s_continuous_buffer_count++] = *entry;
    else
    {
        FlushContinuousBuffer();
        s_continuous_buffer[s_continuous_buffer_count++] = *entry;
    }
}

void DataLogger_Flush(void)
{
    if (s_gesture_buffer_count == 0 && s_continuous_buffer_count == 0) return;
    FlushGestureBuffer();
    FlushContinuousBuffer();
    s_last_flush_ms = HAL_GetTick();
}

void DataLogger_Update(void)
{
    if (HAL_GetTick() - s_last_flush_ms >= FLUSH_INTERVAL_MS)
    {
        if (GestureDetector_GetState() == STATE_IDLE)
            DataLogger_Flush();
    }
}

void DataLogger_ClearGestureLog(void)
{
	FRESULT fres;
    s_gesture_buffer_count = 0;

    if (s_files_open)
    {
        f_close(&s_gesture_file);
        s_files_open = 0;
    }

    char full_path[32];
    snprintf(full_path, sizeof(full_path), "%s%s", s_path, LOG_GESTURE_FILENAME);
    f_unlink(full_path);
    InitLogFile(LOG_GESTURE_FILENAME, LOG_GESTURE_MAX_ENTRIES, sizeof(GestureLogEntry_t));

    fres = f_open(&s_gesture_file, full_path, FA_READ | FA_WRITE);
    if (fres == FR_OK)
    {
        UINT br;
        f_lseek(&s_gesture_file, 0);
        f_read(&s_gesture_file, &s_gesture_header, sizeof(LogFileHeader_t), &br);
        s_files_open = 1;
    }

    printf("DataLogger: gesture log cleared\r\n");
}

void DataLogger_ClearContinuousLog(void)
{
    s_continuous_buffer_count = 0;

    if (s_files_open)
    {
        f_close(&s_continuous_file);
        s_files_open = 0;
    }

    char full_path[32];
    snprintf(full_path, sizeof(full_path), "%s%s", s_path, LOG_CONTINUOUS_FILENAME);
    f_unlink(full_path);
    InitLogFile(LOG_CONTINUOUS_FILENAME, LOG_CONTINUOUS_MAX_ENTRIES, sizeof(ContinuousLogEntry_t));

    FRESULT fres = f_open(&s_continuous_file, full_path, FA_READ | FA_WRITE);
    if (fres == FR_OK) s_files_open = 1;

    printf("DataLogger: continuous log cleared\r\n");
}

void DataLogger_ClearAll(void)
{
    // Close both files first
    if (s_files_open)
    {
        f_close(&s_gesture_file);
        f_close(&s_continuous_file);
        s_files_open = 0;
    }

    s_gesture_buffer_count    = 0;
    s_continuous_buffer_count = 0;

    char full_path[32];

    snprintf(full_path, sizeof(full_path), "%s%s", s_path, LOG_GESTURE_FILENAME);
    f_unlink(full_path);
    InitLogFile(LOG_GESTURE_FILENAME, LOG_GESTURE_MAX_ENTRIES, sizeof(GestureLogEntry_t));

    snprintf(full_path, sizeof(full_path), "%s%s", s_path, LOG_CONTINUOUS_FILENAME);
    f_unlink(full_path);
    InitLogFile(LOG_CONTINUOUS_FILENAME, LOG_CONTINUOUS_MAX_ENTRIES, sizeof(ContinuousLogEntry_t));

    // Reopen both
    snprintf(full_path, sizeof(full_path), "%s%s", s_path, LOG_GESTURE_FILENAME);
    FRESULT fres = f_open(&s_gesture_file, full_path, FA_READ | FA_WRITE);
    if (fres != FR_OK) { printf("DataLogger: ClearAll reopen gesture failed\r\n"); return; }

    snprintf(full_path, sizeof(full_path), "%s%s", s_path, LOG_CONTINUOUS_FILENAME);
    fres = f_open(&s_continuous_file, full_path, FA_READ | FA_WRITE);
    if (fres != FR_OK) { printf("DataLogger: ClearAll reopen continuous failed\r\n"); return; }

    s_files_open = 1;
    printf("DataLogger: all logs cleared\r\n");
}

uint32_t DataLogger_GetLastGestureEntries(GestureLogEntry_t *buffer, uint32_t count)
{
    if (!s_files_open) return 0;

    // Use cached header instead of reading from disk
    if (count > s_gesture_header.count) count = s_gesture_header.count;

    uint32_t start = (s_gesture_header.tail + s_gesture_header.max_entries - count) % s_gesture_header.max_entries;
    uint32_t read_count = 0;
    UINT br;

    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t idx    = (start + i) % s_gesture_header.max_entries;
        uint32_t offset = sizeof(LogFileHeader_t) + (idx * sizeof(GestureLogEntry_t));
        if (f_lseek(&s_gesture_file, offset) != FR_OK) break;
        FRESULT fres = f_read(&s_gesture_file, &buffer[i], sizeof(GestureLogEntry_t), &br);
        if (fres != FR_OK || br != sizeof(GestureLogEntry_t)) break;
        read_count++;
    }
    return read_count;
}

uint32_t DataLogger_GetLastContinuousEntries(ContinuousLogEntry_t *buffer, uint32_t count)
{
    if (!s_files_open) return 0;

    if (count > s_continuous_header.count) count = s_continuous_header.count;

    uint32_t start = (s_continuous_header.tail + s_continuous_header.max_entries - count) % s_continuous_header.max_entries;
    uint32_t read_count = 0;
    UINT br;

    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t idx    = (start + i) % s_continuous_header.max_entries;
        uint32_t offset = sizeof(LogFileHeader_t) + (idx * sizeof(ContinuousLogEntry_t));
        if (f_lseek(&s_continuous_file, offset) != FR_OK) break;
        FRESULT fres = f_read(&s_continuous_file, &buffer[i], sizeof(ContinuousLogEntry_t), &br);
        if (fres != FR_OK || br != sizeof(ContinuousLogEntry_t)) break;
        read_count++;
    }
    return read_count;
}

void DataLogger_PrintGestureEntry(GestureLogEntry_t *entry)
{
    const char *event_str[] = {
        "CONFIRMED", "TIMEOUT", "NEAR_MISS_90", "TOO_SENSITIVE"
    };
    const char *gesture_str[] = {
        "NONE", "FLICK_LEFT", "FLICK_RIGHT", "FLICK_UP",
        "FLICK_DOWN", "ROTATE_CW", "ROTATE_CCW"
    };

    printf("--- Gesture Entry ---\r\n");
    printf("  Time:      %lu ms\r\n",           (unsigned long)entry->timestamp_ms);
    printf("  Event:     %s\r\n",               event_str[entry->event_type]);
    printf("  Gesture:   %s\r\n",               gesture_str[entry->gesture_result]);
    printf("  Peak Mag:  %.1f\r\n",             entry->peak_magnitude);
    printf("  Peak XYZ:  %.1f %.1f %.1f\r\n",   entry->peak_gyro_x, entry->peak_gyro_y, entry->peak_gyro_z);
    printf("  Duration:  %lu ms\r\n",           (unsigned long)entry->duration_ms);
    printf("  Threshold: %.1f\r\n",             entry->config_snapshot.gesture_magnitude_threshold);
    printf("  Window:    %lu ms\r\n",           (unsigned long)entry->config_snapshot.gesture_detection_window_ms);
}

void DataLogger_SaveConfig(void)
{
    FIL fil;
    char full_path[32];
    char fval[20];  // temp buffer for float values
    snprintf(full_path, sizeof(full_path), "%s%s", s_path, CONFIG_FILENAME);

    f_unlink(full_path);
    FRESULT fres = f_open(&fil, full_path, FA_CREATE_NEW | FA_WRITE);
    if (fres != FR_OK)
    {
        printf("DataLogger_SaveConfig: open failed (%d)\r\n", fres);
        return;
    }

    #define WRITE_FLOAT(key, val) \
        snprintf(fval, sizeof(fval), "%.4f", (float)(val)); \
        f_printf(&fil, key "=%s\r\n", fval);

    #define WRITE_INT(key, val) \
        f_printf(&fil, key "=%ld\r\n", (long)(val));

    WRITE_FLOAT("gyro_bias_x",                 gyro_bias_x)
    WRITE_FLOAT("gyro_bias_y",                 gyro_bias_y)
    WRITE_FLOAT("gyro_bias_z",                 gyro_bias_z)
    WRITE_FLOAT("gyro_lpf_alpha",              gyro_lpf_alpha)
    //WRITE_FLOAT("gyro_hpf_alpha",              gyro_hpf_alpha)
    //WRITE_FLOAT("gyro_complementary_alpha",    gyro_complementary_alpha)
    WRITE_FLOAT("gesture_magnitude_threshold", gesture_magnitude_threshold)
    WRITE_INT  ("gesture_detection_window_ms", gesture_detection_window_ms)
    WRITE_INT  ("gesture_cooldown_ms",         gesture_cooldown_ms)
    WRITE_INT  ("gesture_debounce_ms",         gesture_debounce_ms)
    WRITE_FLOAT("gesture_peak_fraction",       gesture_peak_fraction)
    //WRITE_FLOAT("gyro_sensitivity_scale",      gyro_sensitivity_scale)
    //WRITE_FLOAT("gyro_base_sensitivity",       gyro_base_sensitivity)
    //WRITE_FLOAT("gyro_accel_threshold_dps",    gyro_accel_threshold_dps)
    //WRITE_FLOAT("gyro_accel_multiplier",       gyro_accel_multiplier)
    //WRITE_INT  ("gyro_max_delta",              gyro_max_delta)
    //WRITE_INT  ("gyro_update_rate_hz",         gyro_update_rate_hz)
    //WRITE_INT  ("gyro_update_interval_ms",     gyro_update_interval_ms)

    #undef WRITE_FLOAT
    #undef WRITE_INT

    f_close(&fil);
    printf("DataLogger: config saved\r\n");
}

void DataLogger_LoadConfig(void)
{
    FIL fil;
    char full_path[32];
    snprintf(full_path, sizeof(full_path), "%s%s", s_path, CONFIG_FILENAME);

    FRESULT fres = f_open(&fil, full_path, FA_READ);
    if (fres != FR_OK)
    {
        printf("DataLogger_LoadConfig: no config file, saving defaults\r\n");
        DataLogger_SaveConfig();
        return;
    }

    char line[64];
    char key[48];
    char value[16];

    while (f_gets(line, sizeof(line), &fil))
    {
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '#') continue;

        char *eq = strchr(line, '=');
        if (eq == NULL) continue;

        uint8_t key_len = eq - line;
        strncpy(key, line, key_len);
        key[key_len] = '\0';
        strncpy(value, eq + 1, sizeof(value) - 1);
        value[sizeof(value) - 1] = '\0';

        if      (strcmp(key, "gyro_bias_x")                  == 0) gyro_bias_x                  = atoff(value);
        else if (strcmp(key, "gyro_bias_y")                  == 0) gyro_bias_y                  = atoff(value);
        else if (strcmp(key, "gyro_bias_z")                  == 0) gyro_bias_z                  = atoff(value);
        else if (strcmp(key, "gyro_lpf_alpha")               == 0) gyro_lpf_alpha               = atoff(value);
        //else if (strcmp(key, "gyro_hpf_alpha")               == 0) gyro_hpf_alpha               = atoff(value);
        //else if (strcmp(key, "gyro_complementary_alpha")     == 0) gyro_complementary_alpha     = atoff(value);
        else if (strcmp(key, "gesture_magnitude_threshold")  == 0) gesture_magnitude_threshold  = atoff(value);
        else if (strcmp(key, "gesture_detection_window_ms")  == 0) gesture_detection_window_ms  = (uint32_t)atol(value);
        else if (strcmp(key, "gesture_cooldown_ms")          == 0) gesture_cooldown_ms          = (uint32_t)atol(value);
        else if (strcmp(key, "gesture_debounce_ms")          == 0) gesture_debounce_ms          = (uint32_t)atol(value);
        else if (strcmp(key, "gesture_peak_fraction")        == 0) gesture_peak_fraction        = atoff(value);
        //else if (strcmp(key, "gyro_sensitivity_scale")       == 0) gyro_sensitivity_scale       = atoff(value);
        //else if (strcmp(key, "gyro_base_sensitivity")        == 0) gyro_base_sensitivity        = atoff(value);
        //else if (strcmp(key, "gyro_accel_threshold_dps")     == 0) gyro_accel_threshold_dps     = atoff(value);
        //else if (strcmp(key, "gyro_accel_multiplier")        == 0) gyro_accel_multiplier        = atoff(value);
        //else if (strcmp(key, "gyro_max_delta")               == 0) gyro_max_delta               = (int32_t)atol(value);
        //else if (strcmp(key, "gyro_update_rate_hz")          == 0) gyro_update_rate_hz          = (int32_t)atol(value);
        //else if (strcmp(key, "gyro_update_interval_ms")      == 0) gyro_update_interval_ms      = (int32_t)atol(value);
        else printf("DataLogger_LoadConfig: unknown key '%s'\r\n", key);
    }

    f_close(&fil);
    printf("DataLogger: config loaded\r\n");
}

void DataLogger_LogAnalyzerChange(AnalyzerChangeEntry_t *entry)
{
    if (!s_files_open) return;
    WriteEntry(&s_analyzer_file, &s_analyzer_header, entry, sizeof(AnalyzerChangeEntry_t));
    f_lseek(&s_analyzer_file, 0);
    UINT bw;
    f_write(&s_analyzer_file, &s_analyzer_header, sizeof(LogFileHeader_t), &bw);
    f_sync(&s_analyzer_file);
}

uint32_t DataLogger_GetLastAnalyzerEntries(AnalyzerChangeEntry_t *buffer, uint32_t count)
{
    if (!s_files_open) return 0;
    if (count > s_analyzer_header.count) count = s_analyzer_header.count;

    uint32_t start = (s_analyzer_header.tail + s_analyzer_header.max_entries - count) % s_analyzer_header.max_entries;
    uint32_t read_count = 0;
    UINT br;

    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t idx    = (start + i) % s_analyzer_header.max_entries;
        uint32_t offset = sizeof(LogFileHeader_t) + (idx * sizeof(AnalyzerChangeEntry_t));
        if (f_lseek(&s_analyzer_file, offset) != FR_OK) break;
        if (f_read(&s_analyzer_file, &buffer[i], sizeof(AnalyzerChangeEntry_t), &br) != FR_OK) break;
        if (br != sizeof(AnalyzerChangeEntry_t)) break;
        read_count++;
    }
    return read_count;
}

void DataLogger_PrintAnalyzerEntry(AnalyzerChangeEntry_t *entry)
{
    printf("--- Analyzer Change ---\r\n");
    printf("  Time:         %lu ms\r\n",   (unsigned long)entry->timestamp_ms);
    printf("  Threshold:    %.1f -> %.1f\r\n", entry->old_threshold, entry->new_threshold);
    printf("  Window:       %lu -> %lu ms\r\n", (unsigned long)entry->old_window_ms, (unsigned long)entry->new_window_ms);
    printf("  Peak Frac:    %.2f -> %.2f\r\n", entry->old_peak_fraction, entry->new_peak_fraction);
    printf("  Avg Peak:     %.1f\r\n",     entry->avg_peak);
    printf("  Timeout Rate: %.2f\r\n",     entry->timeout_rate);
}

void DataLogger_ClearAnalyzerLog(void)
{
    if (s_files_open) f_close(&s_analyzer_file);

    char full_path[32];
    snprintf(full_path, sizeof(full_path), "%s%s", s_path, LOG_ANALYZER_FILENAME);
    f_unlink(full_path);
    InitLogFile(LOG_ANALYZER_FILENAME, LOG_ANALYZER_MAX_ENTRIES, sizeof(AnalyzerChangeEntry_t));

    FRESULT fres = f_open(&s_analyzer_file, full_path, FA_READ | FA_WRITE);
    if (fres == FR_OK)
    {
        UINT br;
        f_lseek(&s_analyzer_file, 0);
        f_read(&s_analyzer_file, &s_analyzer_header, sizeof(LogFileHeader_t), &br);
    }
}
