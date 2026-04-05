#include "sleep_math.h"

int calculateSleepSecondsFromSchedules(int currentHour, int currentMin, int currentSec, const char* schedules[], int numSchedules) {
    int currentSecondsOfDay = (currentHour * 3600) + (currentMin * 60) + currentSec;
    int minDiff = 24 * 3600 + 1; // Start with a value larger than a day
    int earliestSchedule = 24 * 3600 + 1;

    for (int i = 0; i < numSchedules; i++) {
        String timeStr = String(schedules[i]);
        int h = timeStr.substring(0, timeStr.indexOf(':')).toInt();
        int m = timeStr.substring(timeStr.indexOf(':') + 1).toInt();
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
        // No future schedule today, wrap to the earliest schedule tomorrow
        return (24 * 3600 - currentSecondsOfDay) + earliestSchedule;
    }

    return minDiff;
}
