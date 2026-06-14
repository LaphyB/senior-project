#include "adaptive_analyzer.h"

#define ANALYZER_LOOKBACK  5
#define MIN_WINDOW_MS  300

void AdaptiveAnalyzer_Run(void)
{
	printf("\033[10;1H");
	printf("\033[J");

    static GestureLogEntry_t entries[ANALYZER_LOOKBACK];
    uint32_t count = DataLogger_GetLastGestureEntries(entries, ANALYZER_LOOKBACK);

    if (count == 0)
    {
        printf("Analyzer: no data yet\r\n");
        return;
    }

    // ── Count events ─────────────────────────────────────────────
    uint32_t confirmed     = 0;
    uint32_t timeouts      = 0;
    uint32_t near_miss     = 0;
    uint32_t too_sensitive = 0;

    float avg_peak_confirmed     = 0.0f;
    float avg_peak_timeout       = 0.0f;
    float avg_duration_confirmed = 0.0f;
    float avg_duration_timeout   = 0.0f;
    float avg_peak               = 0.0f;

    for (uint32_t i = 0; i < count; i++)
    {
        switch (entries[i].event_type)
        {
            case LOG_EVENT_GESTURE_CONFIRMED:
                confirmed++;
                avg_peak_confirmed     += entries[i].peak_magnitude;
                avg_duration_confirmed += entries[i].duration_ms;
                break;
            case LOG_EVENT_GESTURE_TIMEOUT:
                timeouts++;
                avg_peak_timeout     += entries[i].peak_magnitude;
                avg_duration_timeout += entries[i].duration_ms;
                break;
            case LOG_EVENT_NEAR_MISS_90:  near_miss++;      break;
            case LOG_EVENT_TOO_SENSITIVE: too_sensitive++;  break;
        }
    }

    if (confirmed > 0) { avg_peak_confirmed /= confirmed; avg_duration_confirmed /= confirmed; }
    if (timeouts  > 0) { avg_peak_timeout   /= timeouts;  avg_duration_timeout   /= timeouts;  }

    uint32_t total        = confirmed + timeouts;
    float    timeout_rate = total > 0 ? (float)timeouts   / (float)total : 0.0f;
    float    success_rate = total > 0 ? (float)confirmed  / (float)total : 0.0f;

    // ── Print summary ─────────────────────────────────────────────
    printf("[ ANALYZER ] last %lu entries\r\n", (unsigned long)count);
    printf("OK:%lu  TO:%lu  NM:%lu  TS:%lu  Suc:%.0f%%\r\n",
        (unsigned long)confirmed, (unsigned long)timeouts,
        (unsigned long)near_miss, (unsigned long)too_sensitive,
        success_rate * 100.0f);
    printf("Thresh:%.0f  Win:%lums  Frac:%.2f\r\n",
        gesture_magnitude_threshold,
        (unsigned long)gesture_detection_window_ms,
        gesture_peak_fraction);
    printf("Cool:%lums  Deb:%lums  LPF:%.2f\r\n",
        (unsigned long)gesture_cooldown_ms,
        (unsigned long)gesture_debounce_ms,
        gyro_lpf_alpha);
    printf("--\r\n");

    if (total == 0)
    {
        printf("Not enough attempts\r\n");
        return;
    }

    uint8_t changed = 0;

    // ── Detection window ─────────────────────────────────────────
    if (timeout_rate > 0.1f)
    {
        float    scale      = expf(timeout_rate * 1.0986f);
        uint32_t new_window = avg_duration_timeout > 0
                            ? (uint32_t)(avg_duration_timeout * scale)
                            : (uint32_t)(gesture_detection_window_ms * scale);

        uint32_t smart_cap = avg_duration_timeout > 0
                           ? (uint32_t)(avg_duration_timeout * 3.0f)
                           : 1500;
        if (smart_cap > 1500) smart_cap = 1500;
        if (new_window > smart_cap) new_window = smart_cap;
        if (new_window < gesture_detection_window_ms) new_window = gesture_detection_window_ms;

        printf("Win:%lu->%lu ms\r\n",
            (unsigned long)gesture_detection_window_ms,
            (unsigned long)new_window);
        gesture_detection_window_ms = new_window;
        changed = 1;
    }
    // ── Shrink window if gestures complete well under it ──────────
    else if (success_rate > 0.8f && avg_duration_confirmed > 0 &&
             avg_duration_confirmed < gesture_detection_window_ms * 0.5f &&
             gesture_detection_window_ms > MIN_WINDOW_MS)
    {
        uint32_t new_window = (uint32_t)(avg_duration_confirmed * 2.0f);
        if (new_window < MIN_WINDOW_MS) new_window = MIN_WINDOW_MS;

        printf("Win:%lu->%lu ms (shrink)\r\n",
            (unsigned long)gesture_detection_window_ms,
            (unsigned long)new_window);
        gesture_detection_window_ms = new_window;
        changed = 1;
    }

    // ── Threshold ────────────────────────────────────────────────
    if (avg_peak_timeout > 0 && avg_peak_confirmed > 0)
        avg_peak = (avg_peak_timeout + avg_peak_confirmed) / 2.0f;
    else if (avg_peak_timeout > 0)
        avg_peak = avg_peak_timeout;
    else if (avg_peak_confirmed > 0)
        avg_peak = avg_peak_confirmed;

    if (avg_peak > 0)
    {
        float ideal = avg_peak * 0.65f;
        if (ideal < DEFAULT_GESTURE_MAGNITUDE_THRESHOLD * 0.5f)
            ideal = DEFAULT_GESTURE_MAGNITUDE_THRESHOLD * 0.5f;

        if (gesture_magnitude_threshold < ideal * 0.7f ||
            gesture_magnitude_threshold > ideal * 1.5f)
        {
            printf("Thresh:%.0f->%.0f\r\n", gesture_magnitude_threshold, ideal);
            gesture_magnitude_threshold = ideal;
            changed = 1;
        }
    }

    // ── Peak fraction ────────────────────────────────────────────
    if (gesture_peak_fraction > 0.3f)
    {
        printf("Frac:%.2f->0.20\r\n", gesture_peak_fraction);
        gesture_peak_fraction = 0.2f;
        changed = 1;
    }

    // ── Too sensitive ────────────────────────────────────────────
    if (too_sensitive > confirmed * 0.5f && avg_peak > 0)
    {
        float new_thresh = gesture_magnitude_threshold * 1.25f;
        printf("Thresh:%.0f->%.0f (false triggers)\r\n",
            gesture_magnitude_threshold, new_thresh);
        gesture_magnitude_threshold = new_thresh;
        changed = 1;
    }

    // ── Tighten threshold if peaks well above ────────────────────
    if (success_rate > 0.8f && confirmed > 5 &&
        avg_peak_confirmed > gesture_magnitude_threshold * 3.0f)
    {
        float new_thresh = gesture_magnitude_threshold * 1.2f;
        printf("Thresh:%.0f->%.0f (tighten)\r\n",
            gesture_magnitude_threshold, new_thresh);
        gesture_magnitude_threshold = new_thresh;
        changed = 1;
    }

    // ── Cooldown adjustment ───────────────────────────────────────
    if (too_sensitive > 2 && gesture_cooldown_ms < 1000)
    {
        uint32_t new_cooldown = gesture_cooldown_ms + 100;
        if (new_cooldown > 1000) new_cooldown = 1000;
        printf("Cool:%lu->%lu ms (too sensitive)\r\n",
            (unsigned long)gesture_cooldown_ms,
            (unsigned long)new_cooldown);
        gesture_cooldown_ms = new_cooldown;
        changed = 1;
    }
    else if (too_sensitive == 0 && success_rate > 0.8f && gesture_cooldown_ms > 300)
    {
        uint32_t new_cooldown = gesture_cooldown_ms - 50;
        if (new_cooldown < 300) new_cooldown = 300;
        printf("Cool:%lu->%lu ms (responsive)\r\n",
            (unsigned long)gesture_cooldown_ms,
            (unsigned long)new_cooldown);
        gesture_cooldown_ms = new_cooldown;
        changed = 1;
    }

    // ── Debounce adjustment ───────────────────────────────────────
    if (too_sensitive > 2 && gesture_debounce_ms < 300)
    {
        uint32_t new_debounce = gesture_debounce_ms + 50;
        if (new_debounce > 300) new_debounce = 300;
        printf("Deb:%lu->%lu ms\r\n",
            (unsigned long)gesture_debounce_ms,
            (unsigned long)new_debounce);
        gesture_debounce_ms = new_debounce;
        changed = 1;
    }
    else if (too_sensitive == 0 && success_rate > 0.8f && gesture_debounce_ms > 50)
    {
        uint32_t new_debounce = gesture_debounce_ms - 25;
        if (new_debounce < 50) new_debounce = 50;
        printf("Deb:%lu->%lu ms\r\n",
            (unsigned long)gesture_debounce_ms,
            (unsigned long)new_debounce);
        gesture_debounce_ms = new_debounce;
        changed = 1;
    }

    // ── LPF alpha adjustment ─────────────────────────────────────
    if (near_miss > confirmed && gyro_lpf_alpha > 0.05f)
    {
        float new_alpha = gyro_lpf_alpha - 0.01f;
        if (new_alpha < 0.05f) new_alpha = 0.05f;
        printf("LPF:%.2f->%.2f (smoother)\r\n", gyro_lpf_alpha, new_alpha);
        gyro_lpf_alpha = new_alpha;
        changed = 1;
    }
    else if (too_sensitive > confirmed && gyro_lpf_alpha < 0.3f)
    {
        float new_alpha = gyro_lpf_alpha + 0.01f;
        if (new_alpha > 0.3f) new_alpha = 0.3f;
        printf("LPF:%.2f->%.2f (less smooth)\r\n", gyro_lpf_alpha, new_alpha);
        gyro_lpf_alpha = new_alpha;
        changed = 1;
    }

    // ── Save and log ─────────────────────────────────────────────
    if (changed)
    {
        printf("Config updated\r\n");
        DataLogger_SaveConfig();

        AnalyzerChangeEntry_t change_entry;
        change_entry.timestamp_ms      = HAL_GetTick();
        change_entry.old_threshold     = entries[0].config_snapshot.gesture_magnitude_threshold;
        change_entry.new_threshold     = gesture_magnitude_threshold;
        change_entry.old_window_ms     = entries[0].config_snapshot.gesture_detection_window_ms;
        change_entry.new_window_ms     = gesture_detection_window_ms;
        change_entry.old_peak_fraction = entries[0].config_snapshot.gesture_peak_fraction;
        change_entry.new_peak_fraction = gesture_peak_fraction;
        change_entry.avg_peak          = avg_peak_confirmed > 0 ? avg_peak_confirmed : avg_peak_timeout;
        change_entry.timeout_rate      = timeout_rate;
        DataLogger_LogAnalyzerChange(&change_entry);
    }
    else
    {
        printf("No changes needed\r\n");
    }
}

#define BREAK_THRESHOLD   25000.0f
#define BREAK_WINDOW_MS   5000
#define BREAK_PEAK_FRAC   0.5f
#define BREAK_COOLDOWN_MS 100
#define BREAK_DEBOUNCE_MS 10
#define BREAK_LPF_ALPHA   0.5f

void AdaptiveAnalyzer_BreakConfig(void)
{
    printf("\033[10;1H");
    printf("\033[J");
    printf("[ BREAK CONFIG ]\r\n");

    gesture_magnitude_threshold = BREAK_THRESHOLD;
    gesture_detection_window_ms = BREAK_WINDOW_MS;
    gesture_peak_fraction        = BREAK_PEAK_FRAC;
    gesture_cooldown_ms          = BREAK_COOLDOWN_MS;
    gesture_debounce_ms          = BREAK_DEBOUNCE_MS;
    gyro_lpf_alpha                = BREAK_LPF_ALPHA;

    printf("Thresh:%.0f  Win:%dms  Frac:%.2f\r\n", BREAK_THRESHOLD, BREAK_WINDOW_MS, BREAK_PEAK_FRAC);
    printf("Cool:%dms  Deb:%dms  LPF:%.2f\r\n", BREAK_COOLDOWN_MS, BREAK_DEBOUNCE_MS, BREAK_LPF_ALPHA);
    printf("Run analyzer to fix\r\n");
}
