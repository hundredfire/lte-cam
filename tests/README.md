# Sleep Calculation Tests

This directory contains unit tests for the `calculateSleepSecondsFromSchedules` function, which is used to determine how long the ESP32 should sleep until its next scheduled wakeup.

## How to run the tests

The tests are written in standard C++ and can be compiled and run in any environment with a C++ compiler (like `g++`).

```bash
g++ test_sleep_calculations.cpp -o test_sleep_calculations
./test_sleep_calculations
```

The tests cover various scenarios including:
- Before all schedules
- Between schedules
- After all schedules (wrapping to the next day)
- Exactly at a schedule time
- Robust parsing of time strings (e.g., "8:30" vs "08:30")
