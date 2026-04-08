#pragma once

// ---------------------------------------------------------------------------
// PubSubClient mock for host-side (native) unit test compilation.
//
// Captures every call to publish() / subscribe() / connect() / disconnect()
// in a singleton MqttMockState so tests can inspect them after the fact.
// Control publish/connect outcomes by setting the relevant bool fields.
//
// Usage:
//   MqttMockState::reset();                           // clean state in SetUp
//   MqttMockState::instance().publish_result = false; // simulate failure
//   // call the function under test
//   EXPECT_EQ(MqttMockState::instance().last_publish_topic, "sensors/...");
// ---------------------------------------------------------------------------

#include <cstdint>
#include <functional>
#include <string>

#include "WiFi.h"  // provides Client

// ---------------------------------------------------------------------------
// Singleton state — shared by all PubSubClient instances in the process.
// ---------------------------------------------------------------------------
struct MqttMockState {
  // Outcome controls
  bool connect_result{true};
  bool publish_result{true};

  // Observable state
  bool connected_val{false};
  int  disconnect_count{0};

  // Last-call captures
  std::string last_connect_id{};
  std::string last_connect_user{};
  std::string last_connect_pass{};
  std::string last_subscribe_topic{};
  std::string last_publish_topic{};
  std::string last_publish_payload{};

  static MqttMockState& instance() {
    static MqttMockState s;
    return s;
  }
  static void reset() { instance() = MqttMockState{}; }
};

// ---------------------------------------------------------------------------
// PubSubClient mock
// ---------------------------------------------------------------------------
class PubSubClient {
public:
  // Configuration — return *this so real chaining compiles.
  PubSubClient& setServer(const char* /*host*/, uint16_t /*port*/) { return *this; }
  PubSubClient& setClient(Client& /*c*/)                           { return *this; }
  bool          setBufferSize(uint16_t /*size*/)                   { return true;  }
  bool          setSocketTimeout(uint16_t /*timeout*/)             { return true;  }
  void          setCallback(std::function<void(char*, uint8_t*, unsigned int)> /*cb*/) {}

  // Connection state
  bool connected() { return MqttMockState::instance().connected_val; }
  int  state()     { return MqttMockState::instance().connected_val ? 0 : -2; }

  // Connect (no-auth)
  bool connect(const char* id) {
    auto& s = MqttMockState::instance();
    s.last_connect_id = id ? id : "";
    s.connected_val   = s.connect_result;
    return s.connect_result;
  }

  // Connect (with credentials — used when MQTT_AUTH==true)
  bool connect(const char* id, const char* user, const char* pass) {
    auto& s = MqttMockState::instance();
    s.last_connect_id   = id   ? id   : "";
    s.last_connect_user = user ? user : "";
    s.last_connect_pass = pass ? pass : "";
    s.connected_val     = s.connect_result;
    return s.connect_result;
  }

  bool subscribe(const char* topic) {
    MqttMockState::instance().last_subscribe_topic = topic ? topic : "";
    return true;
  }

  bool publish(const char* topic, const char* payload) {
    auto& s = MqttMockState::instance();
    s.last_publish_topic   = topic   ? topic   : "";
    s.last_publish_payload = payload ? payload : "";
    return s.publish_result;
  }

  void disconnect() {
    auto& s = MqttMockState::instance();
    s.connected_val = false;
    s.disconnect_count++;
  }

  bool loop() { return true; }
};
