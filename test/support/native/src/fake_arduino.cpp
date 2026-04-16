#include "Arduino.h"

#include <map>

#include "ArduinoOTA.h"
#include "ESPmDNS.h"
#include "Preferences.h"
#include "WiFi.h"

namespace {

unsigned long gMillis = 0;
std::map<uint8_t, int> gDigitalValues;
std::map<uint8_t, int> gAnalogValues;
std::map<uint8_t, std::deque<int>> gAnalogSequences;

}  // namespace

FakeSerialStream Serial;
ESPClass ESP;

void FakeSerialStream::clear() {
  input_.clear();
  output_.clear();
}

void FakeSerialStream::pushInput(const std::string& text) {
  for (char ch : text) {
    input_.push_back(ch);
  }
}

std::string FakeSerialStream::output() const { return output_; }

int FakeSerialStream::available() { return static_cast<int>(input_.size()); }

int FakeSerialStream::read() {
  if (input_.empty()) {
    return -1;
  }

  const char ch = input_.front();
  input_.pop_front();
  return static_cast<unsigned char>(ch);
}

size_t FakeSerialStream::write(uint8_t value) {
  output_ += static_cast<char>(value);
  return 1;
}

String IPAddress::toString() const {
  return String(bytes_[0]) + "." + bytes_[1] + "." + bytes_[2] + "." + bytes_[3];
}

uint64_t ESPClass::getEfuseMac() const { return efuseMac_; }

void ESPClass::setEfuseMacForTest(uint64_t value) { efuseMac_ = value; }

unsigned long millis() { return gMillis; }

void delay(unsigned long ms) { gMillis += ms; }

void pinMode(uint8_t, uint8_t) {}

void digitalWrite(uint8_t pin, uint8_t value) { gDigitalValues[pin] = value; }

int analogRead(uint8_t pin) {
  auto sequenceIt = gAnalogSequences.find(pin);
  if (sequenceIt != gAnalogSequences.end() && !sequenceIt->second.empty()) {
    const int value = sequenceIt->second.front();
    sequenceIt->second.pop_front();
    if (sequenceIt->second.empty()) {
      gAnalogValues[pin] = value;
    }
    return value;
  }

  const auto valueIt = gAnalogValues.find(pin);
  return valueIt == gAnalogValues.end() ? 0 : valueIt->second;
}

void analogReadResolution(uint8_t) {}

void analogSetPinAttenuation(uint8_t, int) {}

extern "C" void configTime(long, int, const char*, const char*, const char*) {}

namespace native_test {

void resetAll() {
  gMillis = 0;
  gDigitalValues.clear();
  gAnalogValues.clear();
  gAnalogSequences.clear();
  Serial.clear();
  ESP.setEfuseMacForTest(0xABCDEF001234ULL);
  clearPreferences();
  WiFi.resetForTest();
  MDNS.resetForTest();
  ArduinoOTA.resetForTest();
}

void setMillis(unsigned long value) { gMillis = value; }

void advanceMillis(unsigned long deltaMs) { gMillis += deltaMs; }

unsigned long currentMillis() { return gMillis; }

void setAnalogValue(uint8_t pin, int value) {
  gAnalogSequences.erase(pin);
  gAnalogValues[pin] = value;
}

void setAnalogSequence(uint8_t pin, const std::vector<int>& values) {
  gAnalogSequences[pin] = std::deque<int>(values.begin(), values.end());
  if (!values.empty()) {
    gAnalogValues[pin] = values.back();
  }
}

int digitalValue(uint8_t pin) {
  const auto it = gDigitalValues.find(pin);
  return it == gDigitalValues.end() ? LOW : it->second;
}

void clearSerial() { Serial.clear(); }

std::string serialOutput() { return Serial.output(); }

void queueSerialInput(const std::string& text) { Serial.pushInput(text); }

}  // namespace native_test
