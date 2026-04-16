#pragma once

#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

using byte = unsigned char;
using boolean = bool;

struct __FlashStringHelper;

#define F(text) reinterpret_cast<const __FlashStringHelper*>(text)

constexpr int OUTPUT = 0x1;
constexpr int INPUT = 0x0;
constexpr int HIGH = 0x1;
constexpr int LOW = 0x0;
constexpr int ADC_11db = 0x3;

class String {
 public:
  String() = default;
  String(const char* value) : value_(value != nullptr ? value : "") {}
  String(const __FlashStringHelper* value)
      : value_(value != nullptr ? reinterpret_cast<const char*>(value) : "") {}
  String(const std::string& value) : value_(value) {}
  String(char value) : value_(1, value) {}

  template <typename T, typename = std::enable_if_t<std::is_arithmetic<T>::value>>
  String(T value) {
    std::ostringstream out;
    out << value;
    value_ = out.str();
  }

  bool isEmpty() const { return value_.empty(); }
  size_t length() const { return value_.length(); }
  void reserve(size_t count) { value_.reserve(count); }
  const char* c_str() const { return value_.c_str(); }

  void replace(const char* from, const char* to) {
    if (from == nullptr || to == nullptr) {
      return;
    }

    const std::string fromText(from);
    if (fromText.empty()) {
      return;
    }

    const std::string toText(to);
    size_t position = 0;
    while ((position = value_.find(fromText, position)) != std::string::npos) {
      value_.replace(position, fromText.length(), toText);
      position += toText.length();
    }
  }

  void remove(size_t index) {
    if (index == 0) {
      value_.clear();
      return;
    }

    if (index < value_.length()) {
      value_.erase(index);
    }
  }

  String& operator+=(const String& other) {
    value_ += other.value_;
    return *this;
  }

  String& operator+=(const char* other) {
    if (other != nullptr) {
      value_ += other;
    }
    return *this;
  }

  String& operator+=(const __FlashStringHelper* other) {
    if (other != nullptr) {
      value_ += reinterpret_cast<const char*>(other);
    }
    return *this;
  }

  String& operator+=(char other) {
    value_ += other;
    return *this;
  }

  template <typename T, typename = std::enable_if_t<std::is_arithmetic<T>::value>>
  String& operator+=(T value) {
    std::ostringstream out;
    out << value;
    value_ += out.str();
    return *this;
  }

  bool operator==(const String& other) const { return value_ == other.value_; }
  bool operator==(const char* other) const { return value_ == (other != nullptr ? other : ""); }
  bool operator!=(const String& other) const { return !(*this == other); }
  bool operator!=(const char* other) const { return !(*this == other); }

  std::string std() const { return value_; }

 private:
  std::string value_;
};

inline String operator+(String left, const String& right) {
  left += right;
  return left;
}

inline String operator+(String left, const char* right) {
  left += right;
  return left;
}

inline String operator+(String left, const __FlashStringHelper* right) {
  left += right;
  return left;
}

template <typename T, typename = std::enable_if_t<std::is_arithmetic<T>::value>>
inline String operator+(String left, T right) {
  left += right;
  return left;
}

class Print {
 public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t value) = 0;

  size_t print(const String& value) { return writeBuffer(value.c_str()); }
  size_t print(const char* value) { return writeBuffer(value != nullptr ? value : ""); }
  size_t print(const __FlashStringHelper* value) {
    return writeBuffer(value != nullptr ? reinterpret_cast<const char*>(value) : "");
  }
  size_t print(char value) { return write(static_cast<uint8_t>(value)); }

  template <typename T, typename = std::enable_if_t<std::is_arithmetic<T>::value>>
  size_t print(T value) {
    std::ostringstream out;
    out << value;
    return writeBuffer(out.str());
  }

  size_t println() { return print("\n"); }

  template <typename T>
  size_t println(const T& value) {
    size_t count = print(value);
    count += println();
    return count;
  }

 protected:
  size_t writeBuffer(const std::string& value) {
    size_t count = 0;
    for (char ch : value) {
      count += write(static_cast<uint8_t>(ch));
    }
    return count;
  }
};

class Stream : public Print {
 public:
  virtual int available() { return 0; }
  virtual int read() { return -1; }
};

class FakeSerialStream : public Stream {
 public:
  void begin(unsigned long) {}
  void clear();
  void pushInput(const std::string& text);
  std::string output() const;

  int available() override;
  int read() override;
  size_t write(uint8_t value) override;

 private:
  std::deque<char> input_;
  std::string output_;
};

extern FakeSerialStream Serial;

class IPAddress {
 public:
  IPAddress() = default;
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : bytes_{a, b, c, d} {}

  String toString() const;

 private:
  uint8_t bytes_[4] = {0, 0, 0, 0};
};

class ESPClass {
 public:
  uint64_t getEfuseMac() const;
  void setEfuseMacForTest(uint64_t value);

 private:
  uint64_t efuseMac_ = 0xABCDEF001234ULL;
};

extern ESPClass ESP;

unsigned long millis();
void delay(unsigned long ms);
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
int analogRead(uint8_t pin);
void analogReadResolution(uint8_t bits);
void analogSetPinAttenuation(uint8_t pin, int attenuation);

template <typename T>
inline T min(T left, T right) {
  return left < right ? left : right;
}

template <typename T>
inline T max(T left, T right) {
  return left > right ? left : right;
}

template <typename T>
inline T constrain(T value, T low, T high) {
  return value < low ? low : (value > high ? high : value);
}

extern "C" void configTime(long, int, const char*, const char*, const char*);
