#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

#include <string>
#include <iostream>

class String {
    std::string internal;
public:
    String(const char* s = "") : internal(s) {}
    String(const String& other) : internal(other.internal) {}

    int indexOf(char c) const {
        size_t pos = internal.find(c);
        return (pos == std::string::npos) ? -1 : (int)pos;
    }

    String substring(int from, int to = -1) const {
        if (from >= (int)internal.length()) return String("");
        if (to == -1 || to > (int)internal.length()) {
            return String(internal.substr(from).c_str());
        }
        return String(internal.substr(from, to - from).c_str());
    }

    int toInt() const {
        try {
            return std::stoi(internal);
        } catch (...) {
            return 0;
        }
    }

    bool operator==(const char* s) const {
        return internal == s;
    }
};

#endif
