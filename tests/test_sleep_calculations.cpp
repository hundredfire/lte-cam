#include <iostream>
#include <cassert>
#include "../lte-cam/sleep_calculations.h"

void test_next_schedule_today() {
    const char* schedules[] = {"10:00", "17:00"};
    int numSchedules = 2;

    // Current time: 08:00:00 -> should sleep until 10:00 (7200 seconds)
    assert(calculateSleepSecondsFromSchedules(8, 0, 0, schedules, numSchedules) == 7200);

    // Current time: 12:00:00 -> should sleep until 17:00 (18000 seconds)
    assert(calculateSleepSecondsFromSchedules(12, 0, 0, schedules, numSchedules) == 18000);

    std::cout << "test_next_schedule_today passed!" << std::endl;
}

void test_wrap_around_to_tomorrow() {
    const char* schedules[] = {"10:00", "17:00"};
    int numSchedules = 2;

    // Current time: 18:00:00 -> should sleep until 10:00 tomorrow
    // 6 hours until midnight (21600) + 10 hours tomorrow (36000) = 57600
    assert(calculateSleepSecondsFromSchedules(18, 0, 0, schedules, numSchedules) == 57600);

    std::cout << "test_wrap_around_to_tomorrow passed!" << std::endl;
}

void test_edge_cases() {
    const char* schedules[] = {"10:00", "17:00"};
    int numSchedules = 2;

    // Current time: 10:00:00 -> Exactly on schedule.
    // The current logic: scheduleSeconds (36000) is NOT > currentSecondsOfDay (36000).
    // So it will look for the next schedule (17:00) or wrap around.
    // 10:00:00 -> next is 17:00 (25200 seconds)
    assert(calculateSleepSecondsFromSchedules(10, 0, 0, schedules, numSchedules) == 25200);

    // Current time: 10:00:01 -> Just after schedule.
    // Next is 17:00 (25199 seconds)
    assert(calculateSleepSecondsFromSchedules(10, 0, 1, schedules, numSchedules) == 25199);

    // Current time: 17:00:00 -> Exactly on last schedule.
    // Should wrap around to 10:00 tomorrow.
    // 7 hours until midnight (25200) + 10 hours tomorrow (36000) = 61200
    assert(calculateSleepSecondsFromSchedules(17, 0, 0, schedules, numSchedules) == 61200);

    std::cout << "test_edge_cases passed!" << std::endl;
}

void test_single_schedule() {
    const char* schedules[] = {"12:00"};
    int numSchedules = 1;

    // Before schedule: 10:00 -> sleep 2 hours (7200)
    assert(calculateSleepSecondsFromSchedules(10, 0, 0, schedules, numSchedules) == 7200);

    // After schedule: 14:00 -> sleep 22 hours (10h to midnight + 12h tomorrow = 79200)
    assert(calculateSleepSecondsFromSchedules(14, 0, 0, schedules, numSchedules) == 79200);

    std::cout << "test_single_schedule passed!" << std::endl;
}

int main() {
    test_next_schedule_today();
    test_wrap_around_to_tomorrow();
    test_edge_cases();
    test_single_schedule();

    std::cout << "\nAll tests passed successfully!" << std::endl;
    return 0;
}
