#include <iostream>
#include <vector>
#include <string>
#include "../lte-cam/sleep_calculations.h"

void simulate_cycles(int startHour, int startMin, int startSec, int days) {
    const char* schedules[] = {"10:00", "17:00"};
    int numSchedules = 2;

    long long totalSeconds = (startHour * 3600) + (startMin * 60) + startSec;

    std::cout << "Starting simulation from " << startHour << ":" << startMin << ":" << startSec << std::endl;

    for (int i = 0; i < days * 2; ++i) {
        int currentHour = (totalSeconds / 3600) % 24;
        int currentMin = (totalSeconds / 60) % 60;
        int currentSec = totalSeconds % 60;

        int sleepSecs = calculateSleepSecondsFromSchedules(currentHour, currentMin, currentSec, schedules, numSchedules);

        std::cout << "Time: " << (currentHour < 10 ? "0" : "") << currentHour << ":"
                  << (currentMin < 10 ? "0" : "") << currentMin << ":"
                  << (currentSec < 10 ? "0" : "") << currentSec
                  << " -> Sleep: " << sleepSecs << "s (" << sleepSecs/3600 << "h " << (sleepSecs%3600)/60 << "m)" << std::endl;

        totalSeconds += sleepSecs;
        // Simulate some processing time
        totalSeconds += 60;
    }
}

int main() {
    simulate_cycles(0, 0, 0, 2);
    std::cout << "\nSimulation with initial offset (06:20):" << std::endl;
    simulate_cycles(6, 20, 0, 2);
    return 0;
}
