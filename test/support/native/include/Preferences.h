#pragma once

#include <map>
#include <string>
#include <variant>

#include "Arduino.h"

class Preferences {
 public:
  using Value = std::variant<int, uint8_t, uint32_t, uint64_t, std::string>;

  bool begin(const char* namespaceName, bool readOnly);

  int getInt(const char* key, int defaultValue = 0) const;
  uint8_t getUChar(const char* key, uint8_t defaultValue = 0) const;
  uint32_t getUInt(const char* key, uint32_t defaultValue = 0) const;
  uint64_t getULong64(const char* key, uint64_t defaultValue = 0) const;
  String getString(const char* key, const char* defaultValue = "") const;

  size_t putInt(const char* key, int value);
  size_t putUChar(const char* key, uint8_t value);
  size_t putUInt(const char* key, uint32_t value);
  size_t putULong64(const char* key, uint64_t value);
  size_t putString(const char* key, const String& value);

  bool isKey(const char* key) const;
  bool remove(const char* key);

  static std::map<std::string, std::map<std::string, Value>>& store();

 private:
  std::string namespaceName_;
  bool begun_ = false;
};

namespace native_test {

void clearPreferences();

}  // namespace native_test
