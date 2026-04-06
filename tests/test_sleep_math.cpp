#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include "sleep_math.h"

void test_calculateSleepSeconds() {
    const char* schedules[] = {"10:00", "17:00"};
    int numSchedules = 2;

    // Test case 1: Current time is before the first schedule
    // 08:00:00 -> 10:00:00 is 2 hours (7200 seconds)
    assert(calculateSleepSecondsFromSchedules(8, 0, 0, schedules, numSchedules) == 7200);

    // Test case 2: Current time is between schedules
    // 12:00:00 -> 17:00:00 is 5 hours (18000 seconds)
    assert(calculateSleepSecondsFromSchedules(12, 0, 0, schedules, numSchedules) == 18000);

    // Test case 3: Current time is after all schedules (wrap around)
    // 19:00:00 -> 10:00:00 next day
    // 5 hours until midnight + 10 hours = 15 hours (54000 seconds)
    assert(calculateSleepSecondsFromSchedules(19, 0, 0, schedules, numSchedules) == 54000);

    // Test case 4: Current time is exactly a schedule time (should find the NEXT one)
    // 10:00:00 -> 17:00:00 is 7 hours (25200 seconds)
    assert(calculateSleepSecondsFromSchedules(10, 0, 0, schedules, numSchedules) == 25200);

    // Test case 5: Current time is exactly the LAST schedule time (should wrap)
    // 17:00:00 -> 10:00:00 next day
    // 7 hours until midnight + 10 hours = 17 hours (61200 seconds)
    assert(calculateSleepSecondsFromSchedules(17, 0, 0, schedules, numSchedules) == 61200);

    // Test case 6: Seconds and minutes offset
    // 09:59:30 -> 10:00:00 is 30 seconds
    assert(calculateSleepSecondsFromSchedules(9, 59, 30, schedules, numSchedules) == 30);

    // Test case 7: Single schedule
    const char* singleSchedule[] = {"12:00"};
    // 08:00:00 -> 12:00:00 is 4 hours (14400 seconds)
    assert(calculateSleepSecondsFromSchedules(8, 0, 0, singleSchedule, 1) == 14400);
    // 14:00:00 -> 12:00:00 next day is 10 + 12 = 22 hours (79200 seconds)
    assert(calculateSleepSecondsFromSchedules(14, 0, 0, singleSchedule, 1) == 79200);

    // Test case 8: Tolerance skipping
    // Target schedule is 10:00. Time is 09:59:00. Tolerance is 2 minutes.
    // Since 09:59 is within tolerance, it skips 10:00 and calculates time until 17:00 (7 hours and 1 minute = 25260 seconds).
    assert(calculateSleepSecondsFromSchedules(9, 59, 0, schedules, numSchedules, 2) == 25260);

    // Target schedule is 10:00. Time is 10:01:00. Tolerance is 2 minutes.
    // Since 10:01 is within tolerance, it skips 10:00 and calculates time until 17:00 (6 hours and 59 minutes = 25140 seconds).
    assert(calculateSleepSecondsFromSchedules(10, 1, 0, schedules, numSchedules, 2) == 25140);

    // Target schedule is 17:00. Time is 16:59:00. Tolerance is 2 minutes.
    // Skip 17:00 and calculate until 10:00 next day (7 hours until midnight + 10 hours = 17 hours and 1 minute = 61260 seconds).
    assert(calculateSleepSecondsFromSchedules(16, 59, 0, schedules, numSchedules, 2) == 61260);

    // Test wrap around with tolerance (Target is 00:01, time is 23:59:00 previous day).
    const char* midnightSchedule[] = {"00:01"};
    // Without tolerance: time to next is 2 minutes = 120 seconds.
    assert(calculateSleepSecondsFromSchedules(23, 59, 0, midnightSchedule, 1, 0) == 120);
    // With 3 minute tolerance, it skips 00:01 today and aims for 00:01 next day (24 hours + 2 minutes = 86520 seconds).
    assert(calculateSleepSecondsFromSchedules(23, 59, 0, midnightSchedule, 1, 3) == 86520);
}

void test_isWithinScheduleGracePeriod() {
    const char* schedules[] = {"10:00", "17:00"};
    int numSchedules = 2;
    int gracePeriodSeconds = 900; // 15 minutes

    // Exact match
    assert(isWithinScheduleGracePeriod(10, 0, 0, schedules, numSchedules, gracePeriodSeconds) == true);

    // Within grace period (5 minutes after)
    assert(isWithinScheduleGracePeriod(10, 5, 0, schedules, numSchedules, gracePeriodSeconds) == true);

    // At the end of grace period (15 minutes after)
    assert(isWithinScheduleGracePeriod(10, 15, 0, schedules, numSchedules, gracePeriodSeconds) == true);

    // Just after grace period (15 minutes and 1 second after)
    assert(isWithinScheduleGracePeriod(10, 15, 1, schedules, numSchedules, gracePeriodSeconds) == false);

    // Before schedule
    assert(isWithinScheduleGracePeriod(9, 59, 59, schedules, numSchedules, gracePeriodSeconds) == false);

    // Between schedules (not within grace)
    assert(isWithinScheduleGracePeriod(12, 0, 0, schedules, numSchedules, gracePeriodSeconds) == false);

    // Within grace of second schedule
    assert(isWithinScheduleGracePeriod(17, 10, 0, schedules, numSchedules, gracePeriodSeconds) == true);
}

int main() {
    test_calculateSleepSeconds();
    test_isWithinScheduleGracePeriod();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
