#include <iostream>
#include <cassert>
#include "../lte-cam/sleep_math.h"

void test_before_schedules() {
    const char* schedules[] = {"10:00", "15:00"};
    int numSchedules = 2;
    // Current time: 08:00:00
    int result = calculateSleepSecondsFromSchedules(8, 0, 0, schedules, numSchedules);
    // Should be 2 hours = 7200 seconds
    assert(result == 7200);
    std::cout << "test_before_schedules passed" << std::endl;
}

void test_between_schedules() {
    const char* schedules[] = {"10:00", "15:00"};
    int numSchedules = 2;
    // Current time: 11:00:00
    int result = calculateSleepSecondsFromSchedules(11, 0, 0, schedules, numSchedules);
    // Should be 4 hours = 14400 seconds
    assert(result == 14400);
    std::cout << "test_between_schedules passed" << std::endl;
}

void test_after_all_schedules() {
    const char* schedules[] = {"10:00", "15:00"};
    int numSchedules = 2;
    // Current time: 16:00:00
    int result = calculateSleepSecondsFromSchedules(16, 0, 0, schedules, numSchedules);
    // Should wrap to 10:00 tomorrow.
    // Time until midnight: 8 hours = 28800
    // Time from midnight to 10:00: 10 hours = 36000
    // Total: 64800
    assert(result == 64800);
    std::cout << "test_after_all_schedules passed" << std::endl;
}

void test_exact_match() {
    const char* schedules[] = {"10:00", "15:00"};
    int numSchedules = 2;
    // Current time: 10:00:00
    int result = calculateSleepSecondsFromSchedules(10, 0, 0, schedules, numSchedules);
    // Logic uses scheduleSeconds > currentSecondsOfDay, so 10:00:00 is not > 10:00:00.
    // It should pick 15:00:00.
    // 5 hours = 18000
    assert(result == 18000);
    std::cout << "test_exact_match passed" << std::endl;
}

void test_single_schedule_before() {
    const char* schedules[] = {"12:00"};
    int numSchedules = 1;
    int result = calculateSleepSecondsFromSchedules(10, 0, 0, schedules, numSchedules);
    assert(result == 7200);
    std::cout << "test_single_schedule_before passed" << std::endl;
}

void test_single_schedule_after() {
    const char* schedules[] = {"12:00"};
    int numSchedules = 1;
    int result = calculateSleepSecondsFromSchedules(14, 0, 0, schedules, numSchedules);
    // 10 hours to midnight + 12 hours tomorrow = 22 hours = 79200
    assert(result == 79200);
    std::cout << "test_single_schedule_after passed" << std::endl;
}

void test_midnight() {
    const char* schedules[] = {"00:00"};
    int numSchedules = 1;
    int result = calculateSleepSecondsFromSchedules(0, 0, 1, schedules, numSchedules);
    // Almost 24 hours
    assert(result == 24 * 3600 - 1);
    std::cout << "test_midnight passed" << std::endl;
}

int main() {
    test_before_schedules();
    test_between_schedules();
    test_after_all_schedules();
    test_exact_match();
    test_single_schedule_before();
    test_single_schedule_after();
    test_midnight();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
