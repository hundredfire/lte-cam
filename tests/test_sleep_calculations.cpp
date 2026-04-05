#include <iostream>
#include <cassert>
#include "../lte-cam/sleep_math.h"

void test_before_all_schedules() {
    const char* schedules[] = {"10:00", "17:00"};
    int numSchedules = 2;
    // Current time: 08:00:00
    int sleep = calculateSleepSecondsFromSchedules(8, 0, 0, schedules, numSchedules);
    // Expected: 10:00:00 - 08:00:00 = 2 hours = 7200 seconds
    assert(sleep == 7200);
    std::cout << "test_before_all_schedules passed!" << std::endl;
}

void test_between_schedules() {
    const char* schedules[] = {"10:00", "17:00"};
    int numSchedules = 2;
    // Current time: 12:00:00
    int sleep = calculateSleepSecondsFromSchedules(12, 0, 0, schedules, numSchedules);
    // Expected: 17:00:00 - 12:00:00 = 5 hours = 18000 seconds
    assert(sleep == 18000);
    std::cout << "test_between_schedules passed!" << std::endl;
}

void test_after_all_schedules() {
    const char* schedules[] = {"10:00", "17:00"};
    int numSchedules = 2;
    // Current time: 19:00:00
    int sleep = calculateSleepSecondsFromSchedules(19, 0, 0, schedules, numSchedules);
    // Expected: Time until 10:00:00 tomorrow
    // (24 - 19) = 5 hours today + 10 hours tomorrow = 15 hours = 54000 seconds
    assert(sleep == 54000);
    std::cout << "test_after_all_schedules passed!" << std::endl;
}

void test_exactly_at_schedule() {
    const char* schedules[] = {"10:00", "17:00"};
    int numSchedules = 2;
    // Current time: 10:00:00
    int sleep = calculateSleepSecondsFromSchedules(10, 0, 0, schedules, numSchedules);
    // Expected: Next schedule is 17:00:00
    // 17:00:00 - 10:00:00 = 7 hours = 25200 seconds
    assert(sleep == 25200);
    std::cout << "test_exactly_at_schedule passed!" << std::endl;
}

void test_wrap_exactly_at_last_schedule() {
    const char* schedules[] = {"10:00", "17:00"};
    int numSchedules = 2;
    // Current time: 17:00:00
    int sleep = calculateSleepSecondsFromSchedules(17, 0, 0, schedules, numSchedules);
    // Expected: Next schedule is 10:00:00 tomorrow
    // (24 - 17) = 7 hours today + 10 hours tomorrow = 17 hours = 61200 seconds
    assert(sleep == 61200);
    std::cout << "test_wrap_exactly_at_last_schedule passed!" << std::endl;
}

void test_single_schedule() {
    const char* schedules[] = {"12:00"};
    int numSchedules = 1;
    // Current time: 08:00:00
    int sleep = calculateSleepSecondsFromSchedules(8, 0, 0, schedules, numSchedules);
    assert(sleep == 14400); // 4 hours

    // Current time: 14:00:00
    sleep = calculateSleepSecondsFromSchedules(14, 0, 0, schedules, numSchedules);
    // (24 - 14) + 12 = 10 + 12 = 22 hours = 79200 seconds
    assert(sleep == 79200);
    std::cout << "test_single_schedule passed!" << std::endl;
}

void test_non_chronological_schedules() {
    const char* schedules[] = {"17:00", "10:00"};
    int numSchedules = 2;
    // Current time: 08:00:00
    int sleep = calculateSleepSecondsFromSchedules(8, 0, 0, schedules, numSchedules);
    assert(sleep == 7200); // Should still pick 10:00

    // Current time: 12:00:00
    sleep = calculateSleepSecondsFromSchedules(12, 0, 0, schedules, numSchedules);
    assert(sleep == 18000); // Should pick 17:00
    std::cout << "test_non_chronological_schedules passed!" << std::endl;
}

void test_robust_parsing() {
    const char* schedules[] = {"8:30", "09:05"};
    int numSchedules = 2;
    // Current time: 08:00:00
    int sleep = calculateSleepSecondsFromSchedules(8, 0, 0, schedules, numSchedules);
    assert(sleep == 1800); // 30 mins

    // Current time: 08:45:00
    sleep = calculateSleepSecondsFromSchedules(8, 45, 0, schedules, numSchedules);
    assert(sleep == 1200); // 20 mins to 09:05
    std::cout << "test_robust_parsing passed!" << std::endl;
}

int main() {
    test_before_all_schedules();
    test_between_schedules();
    test_after_all_schedules();
    test_exactly_at_schedule();
    test_wrap_exactly_at_last_schedule();
    test_single_schedule();
    test_non_chronological_schedules();
    test_robust_parsing();

    std::cout << "All tests passed successfully!" << std::endl;
    return 0;
}
