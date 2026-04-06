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
