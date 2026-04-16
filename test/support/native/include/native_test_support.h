#pragma once

#include <vector>

#include "Arduino.h"

namespace native_test {

void resetAll();

void setMillis(unsigned long value);
void advanceMillis(unsigned long deltaMs);
unsigned long currentMillis();

void setAnalogValue(uint8_t pin, int value);
void setAnalogSequence(uint8_t pin, const std::vector<int>& values);
int digitalValue(uint8_t pin);

void clearSerial();
std::string serialOutput();
void queueSerialInput(const std::string& text);

}  // namespace native_test
