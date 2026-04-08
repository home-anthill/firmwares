#pragma once

// ---------------------------------------------------------------------------
// Preferences mock for host-side (native) unit test compilation.
//
// Simulates ESP32 NVS (Non-Volatile Storage) via in-memory maps.
// All Preferences instances share the same backing store (matching real NVS
// behaviour where data persists across begin/end calls).
//
// Call Preferences::reset() in test SetUp() to start each test with a clean
// store.
// ---------------------------------------------------------------------------

#include <cstdint>
#include <cstring>
#include <map>
#include <string>

namespace pref_store {
  inline std::map<std::string, std::string> strings;
  inline std::map<std::string, int32_t>     ints;
}

class Preferences {
public:
  void begin(const char* /*ns*/, bool /*readOnly*/ = false) {}
  void end() {}

  // Wipe all stored values — call from test SetUp.
  static void reset() {
    pref_store::strings.clear();
    pref_store::ints.clear();
  }

  // putString(key, c-string) → bytes stored
  size_t putString(const char* key, const char* value) {
    pref_store::strings[key] = value;
    return strlen(value);
  }

  // putString(key, std::string) → bytes stored
  size_t putString(const char* key, const std::string& value) {
    pref_store::strings[key] = value;
    return value.size();
  }

  // getString(key, buf, maxLen) → chars in stored string (0 if missing)
  size_t getString(const char* key, char* buf, size_t maxLen) {
    auto it = pref_store::strings.find(key);
    if (it == pref_store::strings.end()) {
      if (maxLen > 0) buf[0] = '\0';
      return 0;
    }
    strncpy(buf, it->second.c_str(), maxLen);
    if (maxLen > 0) buf[maxLen - 1] = '\0';
    return it->second.size();
  }

  // getString(key, defaultVal) → std::string (== Arduino String)
  std::string getString(const char* key, const char* defaultVal = "") {
    auto it = pref_store::strings.find(key);
    return (it != pref_store::strings.end()) ? it->second : std::string(defaultVal);
  }

  size_t putInt(const char* key, int32_t value) {
    pref_store::ints[key] = value;
    return sizeof(int32_t);
  }

  int32_t getInt(const char* key, int32_t defaultVal = 0) {
    auto it = pref_store::ints.find(key);
    return (it != pref_store::ints.end()) ? it->second : defaultVal;
  }
};
