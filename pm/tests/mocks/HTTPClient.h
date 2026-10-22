#pragma once

// ---------------------------------------------------------------------------
// HTTPClient mock for host-side (native) unit test compilation.
//
// Arduino.h (which defines Stream) and WiFi.h (which defines WiFiClient)
// must be on the include path and resolved before this header is processed.
//
// Usage in tests:
//   HttpMockConfig::reset();                        // clean state
//   HttpMockConfig::instance().response_code = 200;
//   HttpMockConfig::instance().response_body = R"({"uuid":"..."})";
//   // call the function under test
//   EXPECT_EQ(HttpMockConfig::instance().last_payload, "...");
// ---------------------------------------------------------------------------

// Arduino.h must be resolved first so that the Stream base class is defined
// before StringStream inherits from it.
#include <Arduino.h>
#include "WiFi.h"

#include <cstdint>
#include <string>

// HTTP status codes used by registration.cpp
#define HTTP_CODE_OK       200
#define HTTP_CODE_CONFLICT 409

// ---------------------------------------------------------------------------
// StringStream — a Stream that reads back a pre-loaded std::string.
// Used by HTTPClient::getStream() to feed JSON to deserializeJson().
// ---------------------------------------------------------------------------
class StringStream : public Stream {
  std::string data_;
  size_t      pos_{0};
public:
  StringStream() = default;

  void set(const std::string& s) { data_ = s; pos_ = 0; }

  int  read()      override { return pos_ < data_.size() ? static_cast<uint8_t>(data_[pos_++]) : -1; }
  int  peek()      override { return pos_ < data_.size() ? static_cast<uint8_t>(data_[pos_])   : -1; }
  int  available() override { return static_cast<int>(data_.size() - pos_); }
  size_t write(uint8_t /*b*/) override { return 0; }
};

// ---------------------------------------------------------------------------
// Singleton config — set before calling the function under test.
// ---------------------------------------------------------------------------
struct HttpMockConfig {
  int         response_code{200};
  std::string response_body{};
  std::string last_url{};
  std::string last_payload{};

  static HttpMockConfig& instance() {
    static HttpMockConfig s;
    return s;
  }
  static void reset() { instance() = HttpMockConfig{}; }
};

inline StringStream g_response_stream;

// ---------------------------------------------------------------------------
// HTTPClient mock — thin façade over HttpMockConfig / g_response_stream.
// ---------------------------------------------------------------------------
class HTTPClient {
public:
  void begin(WiFiClient& /*client*/, const char* url) {
    HttpMockConfig::instance().last_url = url ? url : "";
  }
  void addHeader(const char* /*name*/, const char* /*value*/) {}

  int POST(const char* payload) {
    HttpMockConfig::instance().last_payload = payload ? payload : "";
    g_response_stream.set(HttpMockConfig::instance().response_body);
    return HttpMockConfig::instance().response_code;
  }

  Stream& getStream() { return g_response_stream; }
  void    end()       {}
};
