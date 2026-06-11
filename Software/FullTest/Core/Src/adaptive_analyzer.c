#include "adaptive_analyzer.h"

#define ANALYZER_LOOKBACK  5

void AdaptiveAnalyzer_Run(void)
{
	printf("\033[18;1H");
	printf("\033[J");  // clear from cursor to end of screen
    printf("\r\n<<-------------- ADAPTIVE ANALYZER ------------->>\r\n");

    static GestureLogEntry_t entries[ANALYZER_LOOKBACK];
    uint32_t count = DataLogger_GetLastGestureEntries(entries, ANALYZER_LOOKBACK);

    if (count == 0)
    {
        printf("Analyzer: no gesture data to analyze yet\r\n");
        printf("--------------------------------------------------\r\n\r\n");
        return;
    }

    printf("Analyzer: examining last %lu entries\r\n", (unsigned long)count);

    uint32_t confirmed     = 0;
    uint32_t timeouts      = 0;
    uint32_t near_miss     = 0;
    uint32_t too_sensitive = 0;

    float avg_peak_confirmed     = 0.0f;
    float avg_peak_timeout       = 0.0f;
    float avg_duration_confirmed = 0.0f;
    float avg_duration_timeout   = 0.0f;
    float avg_peak = 0.0f;

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

    if (confirmed > 0) { avg_peak_confirmed     /= confirmed; avg_duration_confirmed /= confirmed; }
    if (timeouts  > 0) { avg_peak_timeout       /= timeouts;  avg_duration_timeout   /= timeouts;  }

    printf("--- Summary ---\r\n");
    printf("  Confirmed:     %lu\r\n", (unsigned long)confirmed);
    printf("  Timeouts:      %lu\r\n", (unsigned long)timeouts);
    printf("  Near misses:   %lu\r\n", (unsigned long)near_miss);
    printf("  Too sensitive: %lu\r\n", (unsigned long)too_sensitive);

    if (confirmed > 0)
        printf("  Avg confirmed peak: %.1f  duration: %.0f ms\r\n",
               avg_peak_confirmed, avg_duration_confirmed);
    if (timeouts > 0)
        printf("  Avg timeout peak:   %.1f  duration: %.0f ms\r\n",
               avg_peak_timeout, avg_duration_timeout);

    printf("--- Current Config ---\r\n");
    printf("  Threshold:  %.1f\r\n",   gesture_magnitude_threshold);
    printf("  Window:     %lu ms\r\n", (unsigned long)gesture_detection_window_ms);
    printf("  Peak frac:  %.2f\r\n",   gesture_peak_fraction);

    printf("--- Suggestions ---\r\n");
    uint8_t changed = 0;
    uint32_t total = confirmed + timeouts;

    if (total == 0)
    {
        printf("  Not enough gesture attempts to analyze\r\n");
    }
    else
    {
        float timeout_rate = (float)timeouts / (float)total;
        float success_rate = (float)confirmed / (float)total;

        printf("  Success rate: %.0f%%  Timeout rate: %.0f%%\r\n",
               success_rate * 100.0f, timeout_rate * 100.0f);

        // ── Fix detection window ──────────────────────────────────
        if (timeout_rate > 0.1f)
        {
            float scale = expf(timeout_rate * 1.0986f);
            uint32_t new_window;

            if (avg_duration_timeout > 0)
                new_window = (uint32_t)(avg_duration_timeout * scale);
            else
                new_window = (uint32_t)(gesture_detection_window_ms * scale);

            // Cap at 3x the average observed duration, never more than 1500ms
            uint32_t smart_cap = (avg_duration_timeout > 0)
                               ? (uint32_t)(avg_duration_timeout * 3.0f)
                               : 1500;
            if (smart_cap > 1500) smart_cap = 1500;
            if (new_window > smart_cap) new_window = smart_cap;

            // Don't reduce the window if it's already reasonable
            if (new_window < gesture_detection_window_ms)
                new_window = gesture_detection_window_ms;

            printf("  [CHANGE] Window: %lu -> %lu ms (scale=%.2fx at %.0f%% timeout rate)\r\n",
                   (unsigned long)gesture_detection_window_ms,
                   (unsigned long)new_window,
                   scale,
                   timeout_rate * 100.0f);
            gesture_detection_window_ms = new_window;
            changed = 1;
        }

        // ── Fix threshold based on observed peaks ─────────────────
        if (avg_peak_timeout > 0 && avg_peak_confirmed > 0)
            avg_peak = (avg_peak_timeout + avg_peak_confirmed) / 2.0f;
        else if (avg_peak_timeout > 0)
            avg_peak = avg_peak_timeout;
        else if (avg_peak_confirmed > 0)
            avg_peak = avg_peak_confirmed;

        if (avg_peak > 0)
        {
            // Threshold should be 45% of average peak for reliable detection
            float ideal_threshold = avg_peak * 0.65f;

            // Also enforce a minimum - threshold should never be below
            // what we know works from the default config
            if (ideal_threshold < DEFAULT_GESTURE_MAGNITUDE_THRESHOLD * 0.5f)
                ideal_threshold = DEFAULT_GESTURE_MAGNITUDE_THRESHOLD * 0.5f;

            if (gesture_magnitude_threshold < ideal_threshold * 0.7f ||
                gesture_magnitude_threshold > ideal_threshold * 1.5f)
            {
                printf("  [CHANGE] Threshold: %.1f -> %.1f (65%% of avg peak %.1f)\r\n",
                       gesture_magnitude_threshold, ideal_threshold, avg_peak);
                gesture_magnitude_threshold = ideal_threshold;
                changed = 1;
            }
        }

        // ── Fix peak fraction if too high ─────────────────────────
        if (gesture_peak_fraction > 0.3f)
        {
            printf("  [CHANGE] Peak fraction: %.2f -> 0.20 (too high, gestures won't complete)\r\n",
                   gesture_peak_fraction);
            gesture_peak_fraction = 0.2f;
            changed = 1;
        }

        // ── Too sensitive - false triggers during idle ────────────
        if (too_sensitive > confirmed * 0.5f && avg_peak > 0)
        {
            float new_threshold = gesture_magnitude_threshold * 1.25f;
            printf("  [CHANGE] Threshold: %.1f -> %.1f (too many false triggers)\r\n",
                   gesture_magnitude_threshold, new_threshold);
            gesture_magnitude_threshold = new_threshold;
            changed = 1;
        }

        // ── Good config - optionally tighten threshold ────────────
        if (success_rate > 0.8f && confirmed > 5 &&
            avg_peak_confirmed > gesture_magnitude_threshold * 3.0f)
        {
            float new_threshold = gesture_magnitude_threshold * 1.2f;
            printf("  [CHANGE] Threshold: %.1f -> %.1f (peaks well above threshold, tighten up)\r\n",
                   gesture_magnitude_threshold, new_threshold);
            gesture_magnitude_threshold = new_threshold;
            changed = 1;
        }
    }

    if (changed)
        {
            printf("  Config updated, changes will take effect immediately\r\n");
            DataLogger_SaveConfig();

            // Log the change
            AnalyzerChangeEntry_t change_entry;
            change_entry.timestamp_ms    = HAL_GetTick();
            change_entry.old_threshold   = entries[0].config_snapshot.gesture_magnitude_threshold;
            change_entry.new_threshold   = gesture_magnitude_threshold;
            change_entry.old_window_ms   = entries[0].config_snapshot.gesture_detection_window_ms;
            change_entry.new_window_ms   = gesture_detection_window_ms;
            change_entry.old_peak_fraction = entries[0].config_snapshot.gesture_peak_fraction;
            change_entry.new_peak_fraction = gesture_peak_fraction;
            change_entry.avg_peak        = (avg_peak_confirmed > 0) ? avg_peak_confirmed : avg_peak_timeout;
            change_entry.timeout_rate    = (total > 0) ? (float)timeouts / (float)total : 0.0f;
            DataLogger_LogAnalyzerChange(&change_entry);
        }
        else
            printf("  No changes needed!\r\n");

    printf("--------------------------------------------------\r\n\r\n");
}

void AdaptiveAnalyzer_BreakConfig(void)
{
	printf("\033[18;1H");
	printf("\033[J");  // clear from cursor to end of screen
    printf("\r\n<<----------------Scrambling Config-------------->>\r\n");

    gesture_magnitude_threshold  = 500.0f;
    gesture_detection_window_ms  = 200;   // too short, will timeout
    gesture_peak_fraction        = 0.5f;  // too high, gesture won't complete

    printf("  Threshold: %.1f -> 500.0 (too low, will false trigger)\r\n", gesture_magnitude_threshold);
    printf("  Window: set to 200ms (too short)\r\n");
    printf("  Peak fraction: set to 0.5 (too high)\r\n");
    printf("  Run analyzer to fix!\r\n\r\n");
    printf("--------------------------------------------------\r\n\r\n");
}
