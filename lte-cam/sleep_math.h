#ifndef SLEEP_MATH_H
#define SLEEP_MATH_H

#include <cstdio>
#include <cstdlib>
#include <cstring>

/**
 * Calculates the number of seconds until the next scheduled time.
 * @param currentHour Current hour (0-23)
 * @param currentMin Current minute (0-59)
 * @param currentSec Current second (0-59)
 * @param schedules Array of schedule strings in "HH:mm" format
 * @param numSchedules Number of schedules in the array
 * @return Seconds until next schedule
 */
inline int calculateSleepSecondsFromSchedules(int currentHour, int currentMin, int currentSec, const char** schedules, int numSchedules) {
    int currentSecondsOfDay = (currentHour * 3600) + (currentMin * 60) + currentSec;
    int minDiff = 24 * 3600 + 1; // Start with a value larger than a day
    int earliestSchedule = 24 * 3600 + 1;

    for (int i = 0; i < numSchedules; i++) {
        int h = 0, m = 0;
        // Robust parsing handling "8:0" or "08:00"
        const char* s = schedules[i];
        char* colon;
        h = strtol(s, &colon, 10);
        if (colon != s && *colon == ':') {
            m = strtol(colon + 1, NULL, 10);
        } else {
            // Fallback or handle invalid format if necessary
            // For now, if sscanf-like parsing fails, we skip or use what we got
        }

        int scheduleSeconds = h * 3600 + m * 60;

        if (scheduleSeconds < earliestSchedule) {
            earliestSchedule = scheduleSeconds;
        }

        if (scheduleSeconds > currentSecondsOfDay) {
            int diff = scheduleSeconds - currentSecondsOfDay;
            if (diff < minDiff) {
                minDiff = diff;
            }
        }
    }

    if (minDiff > 24 * 3600) {
        if (numSchedules == 0) return 3600; // Default to 1 hour if no schedules
        // No future schedule today, wrap to the earliest schedule tomorrow
        return (24 * 3600 - currentSecondsOfDay) + earliestSchedule;
    }

    return minDiff;
}

#endif // SLEEP_MATH_H
