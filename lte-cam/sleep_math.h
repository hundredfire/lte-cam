#pragma once

#include <cstdio>

/**
 * Calculates the number of seconds until the next scheduled wake time.
 *
 * @param currentHour The current hour (0-23).
 * @param currentMin The current minute (0-59).
 * @param currentSec The current second (0-59).
 * @param schedules An array of schedule strings in "HH:mm" format.
 * @param numSchedules The number of elements in the schedules array.
 * @param toleranceMinutes The number of minutes before or after a schedule that it is considered "fulfilled".
 * @return The number of seconds until the next scheduled time.
 */
inline int calculateSleepSecondsFromSchedules(int currentHour, int currentMin, int currentSec, const char* schedules[], int numSchedules, int toleranceMinutes = 0) {
    int currentSecondsOfDay = (currentHour * 3600) + (currentMin * 60) + currentSec;
    int minDiff = 24 * 3600 * 2; // Initialize with a very large value
    int toleranceSeconds = toleranceMinutes * 60;

    for (int i = 0; i < numSchedules; i++) {
        int h = 0, m = 0;
        if (sscanf(schedules[i], "%d:%d", &h, &m) == 2) {
            int scheduleSeconds = h * 3600 + m * 60;

            int diff;
            if (scheduleSeconds > currentSecondsOfDay) {
                diff = scheduleSeconds - currentSecondsOfDay;
            } else {
                diff = (scheduleSeconds + 24 * 3600) - currentSecondsOfDay;
            }

            // If the schedule is upcoming within the tolerance window,
            // we consider it fulfilled for this cycle and skip to its next occurrence tomorrow.
            if (diff > 0 && diff <= toleranceSeconds) {
                diff += 24 * 3600;
            }

            if (diff > 0 && diff < minDiff) {
                minDiff = diff;
            }
        }
    }

    return minDiff;
}

/**
 * Checks if the current time is within a grace period after any scheduled time.
 *
 * @param currentHour The current hour (0-23).
 * @param currentMin The current minute (0-59).
 * @param currentSec The current second (0-59).
 * @param schedules An array of schedule strings in "HH:mm" format.
 * @param numSchedules The number of elements in the schedules array.
 * @param gracePeriodSeconds The maximum number of seconds after a schedule to still trigger.
 * @return True if the current time is within the grace period for any schedule.
 */
inline bool isWithinScheduleGracePeriod(int currentHour, int currentMin, int currentSec, const char* schedules[], int numSchedules, int gracePeriodSeconds) {
    int currentSecondsOfDay = (currentHour * 3600) + (currentMin * 60) + currentSec;

    for (int i = 0; i < numSchedules; i++) {
        int h = 0, m = 0;
        if (sscanf(schedules[i], "%d:%d", &h, &m) == 2) {
            int scheduleSeconds = h * 3600 + m * 60;
            int diff = currentSecondsOfDay - scheduleSeconds;

            // Check if we are within the grace period (0 to gracePeriodSeconds)
            if (diff >= 0 && diff <= gracePeriodSeconds) {
                return true;
            }
        }
    }
    return false;
}
