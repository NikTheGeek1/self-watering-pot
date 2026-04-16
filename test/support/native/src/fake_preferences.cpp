#include "Preferences.h"

namespace {

template <typename T>
T getOrDefault(const std::map<std::string, Preferences::Value>& values, const char* key, T fallback) {
  const auto it = values.find(key != nullptr ? key : "");
  if (it == values.end()) {
    return fallback;
  }

  if (const auto typed = std::get_if<T>(&it->second)) {
    return *typed;
  }

  return fallback;
}

std::string namespaceKey(const char* value) { return value != nullptr ? value : ""; }

}  // namespace

std::map<std::string, std::map<std::string, Preferences::Value>>& Preferences::store() {
  static std::map<std::string, std::map<std::string, Value>> values;
  return values;
}

bool Preferences::begin(const char* namespaceName, bool) {
  namespaceName_ = namespaceKey(namespaceName);
  begun_ = true;
  return true;
}

int Preferences::getInt(const char* key, int defaultValue) const {
  return getOrDefault<int>(store()[namespaceName_], key, defaultValue);
}

uint8_t Preferences::getUChar(const char* key, uint8_t defaultValue) const {
  return getOrDefault<uint8_t>(store()[namespaceName_], key, defaultValue);
}

uint32_t Preferences::getUInt(const char* key, uint32_t defaultValue) const {
  return getOrDefault<uint32_t>(store()[namespaceName_], key, defaultValue);
}

uint64_t Preferences::getULong64(const char* key, uint64_t defaultValue) const {
  return getOrDefault<uint64_t>(store()[namespaceName_], key, defaultValue);
}

String Preferences::getString(const char* key, const char* defaultValue) const {
  const auto it = store()[namespaceName_].find(key != nullptr ? key : "");
  if (it == store()[namespaceName_].end()) {
    return String(defaultValue);
  }

  if (const auto value = std::get_if<std::string>(&it->second)) {
    return String(*value);
  }

  return String(defaultValue);
}

size_t Preferences::putInt(const char* key, int value) {
  store()[namespaceName_][key != nullptr ? key : ""] = value;
  return sizeof(value);
}

size_t Preferences::putUChar(const char* key, uint8_t value) {
  store()[namespaceName_][key != nullptr ? key : ""] = value;
  return sizeof(value);
}

size_t Preferences::putUInt(const char* key, uint32_t value) {
  store()[namespaceName_][key != nullptr ? key : ""] = value;
  return sizeof(value);
}

size_t Preferences::putULong64(const char* key, uint64_t value) {
  store()[namespaceName_][key != nullptr ? key : ""] = value;
  return sizeof(value);
}

size_t Preferences::putString(const char* key, const String& value) {
  store()[namespaceName_][key != nullptr ? key : ""] = value.std();
  return value.length();
}

bool Preferences::isKey(const char* key) const {
  return store()[namespaceName_].count(key != nullptr ? key : "") > 0;
}

bool Preferences::remove(const char* key) {
  return store()[namespaceName_].erase(key != nullptr ? key : "") > 0;
}

namespace native_test {

void clearPreferences() { Preferences::store().clear(); }

}  // namespace native_test
