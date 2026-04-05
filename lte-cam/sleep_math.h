#ifndef SLEEP_MATH_H
#define SLEEP_MATH_H

#include <Arduino.h>

int calculateSleepSecondsFromSchedules(int currentHour, int currentMin, int currentSec, const char* schedules[], int numSchedules);

#endif
