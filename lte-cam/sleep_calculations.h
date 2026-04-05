#ifndef SLEEP_CALCULATIONS_H
#define SLEEP_CALCULATIONS_H

#include <cstdio>

/**
 * Calculates the number of seconds to sleep until the next scheduled time.
 *
 * @param currentHour Current hour (0-23)
 * @param currentMin Current minute (0-59)
 * @param currentSec Current second (0-59)
 * @param schedules Array of schedule strings in "HH:mm" format
 * @param numSchedules Number of schedules in the array
 * @return Number of seconds until the next schedule
 */
static inline int calculateSleepSecondsFromSchedules(int currentHour, int currentMin, int currentSec, const char* schedules[], int numSchedules) {
    int currentSecondsOfDay = (currentHour * 3600) + (currentMin * 60) + currentSec;
    int minDiff = 24 * 3600 + 1; // Start with a value larger than a day
    int earliestSchedule = 24 * 3600 + 1;
    bool foundValid = false;

    for (int i = 0; i < numSchedules; i++) {
        int h = 0, m = 0;
        // Robust parsing with sscanf
        if (schedules[i] && sscanf(schedules[i], "%d:%d", &h, &m) == 2) {
            foundValid = true;
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
    }

    if (!foundValid) {
        // Fallback if no valid schedules were found
        return 3600;
    }

    if (minDiff > 24 * 3600) {
        // No future schedule today, wrap to the earliest schedule tomorrow
        return (24 * 3600 - currentSecondsOfDay) + earliestSchedule;
    }

    return minDiff;
}

#endif // SLEEP_CALCULATIONS_H
