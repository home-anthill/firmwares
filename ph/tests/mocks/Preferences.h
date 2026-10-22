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
#include <vector>
#include <algorithm>

namespace pref_store {
  inline std::map<std::string, std::vector<uint8_t>> bytes;
  inline bool begin_success = true;
  inline bool write_success = true;
  inline int byte_writes = 0;
  inline std::map<std::string, std::string> strings;
  inline std::map<std::string, int32_t>     ints;
}

class Preferences {
public:
  bool begin(const char* ns, bool = false) { namespace_ = ns; return pref_store::begin_success; }
  void end() {}

  // Wipe all stored values — call from test SetUp.
  static void reset() {
    pref_store::bytes.clear();
    pref_store::begin_success = true;
    pref_store::write_success = true;
    pref_store::byte_writes = 0;
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
  size_t getBytesLength(const char* key) {
    return pref_store::bytes[namespace_ + "/" + key].size();
  }
  size_t getBytes(const char* key, void* destination, size_t size) {
    const auto& bytes = pref_store::bytes[namespace_ + "/" + key];
    size_t length = std::min(size, bytes.size());
    if (length) {
      memcpy(destination, bytes.data(), length);
    }
    return length;
  }
  size_t putBytes(const char* key, const void* data, size_t size) {
    ++pref_store::byte_writes;
    if (!pref_store::write_success) {
      return 0;
    }
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    pref_store::bytes[namespace_ + "/" + key] = std::vector<uint8_t>(bytes, bytes + size);
    return size;
  }
private:
  std::string namespace_;
};
