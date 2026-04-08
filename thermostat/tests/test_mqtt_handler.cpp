#include <gtest/gtest.h>
#include <cstring>

// Mock headers must precede production source includes.
#include "WiFi.h"
#include "PubSubClient.h"

#include <ArduinoJson.h>
#include "mqtt_handler.h"
#include "secrets.h"  // provides API_TOKEN, MQTT_USERNAME, MQTT_PASSWORD

// ---------------------------------------------------------------------------
// Fixture — resets mock state before every test.
// ---------------------------------------------------------------------------

class MqttHandlerTest : public ::testing::Test {
protected:
  void SetUp() override {
    MqttMockState::reset();
  }
};

// ===========================================================================
// mqtt_notify_value — topic routing
// ===========================================================================

TEST_F(MqttHandlerTest, NotifyValueUsesSensorTopicFormat) {
  mqtt_notify_value("dev-uuid", "feat-uuid", "temperature", 23.5f);
  EXPECT_EQ(MqttMockState::instance().last_publish_topic, "sensors/dev-uuid/temperature");
}

TEST_F(MqttHandlerTest, NotifyValueUsesOnlineTopicForOnlineType) {
  mqtt_notify_value("dev-uuid", "feat-uuid", "online", 1.0f);
  EXPECT_EQ(MqttMockState::instance().last_publish_topic,
            "online/dev-uuid/features/feat-uuid");
}

// ===========================================================================
// mqtt_notify_value — payload structure
// ===========================================================================

TEST_F(MqttHandlerTest, NotifyValuePayloadStructure) {
  mqtt_notify_value("dev-uuid", "feature-uuid-456", "temperature", 23.5f);

  JsonDocument doc;
  deserializeJson(doc, MqttMockState::instance().last_publish_payload);
  EXPECT_STREQ(doc["apiToken"].as<const char*>(), API_TOKEN);
  EXPECT_STREQ(doc["deviceUuid"].as<const char*>(), "dev-uuid");
  EXPECT_STREQ(doc["featureUuid"].as<const char*>(), "feature-uuid-456");
  EXPECT_FLOAT_EQ(doc["payload"]["value"].as<float>(), 23.5f);
}

TEST_F(MqttHandlerTest, NotifyValuePayloadEncodesNegativeValue) {
  mqtt_notify_value("dev-uuid", "feat-uuid", "temperature", -5.0f);

  JsonDocument doc;
  deserializeJson(doc, MqttMockState::instance().last_publish_payload);
  EXPECT_FLOAT_EQ(doc["payload"]["value"].as<float>(), -5.0f);
}

// ===========================================================================
// mqtt_notify_value — disconnect on failure
// ===========================================================================

TEST_F(MqttHandlerTest, NotifyValueDisconnectsOnPublishFailure) {
  MqttMockState::instance().publish_result = false;
  mqtt_notify_value("dev-uuid", "feat-uuid", "temperature", 23.5f);
  EXPECT_EQ(MqttMockState::instance().disconnect_count, 1);
}

TEST_F(MqttHandlerTest, NotifyValueDoesNotDisconnectOnPublishSuccess) {
  MqttMockState::instance().publish_result = true;
  mqtt_notify_value("dev-uuid", "feat-uuid", "temperature", 23.5f);
  EXPECT_EQ(MqttMockState::instance().disconnect_count, 0);
}

// ===========================================================================
// mqtt_connect — happy path
// secrets.h has MQTT_AUTH true, so connect(id, user, pass) is used.
// ===========================================================================

TEST_F(MqttHandlerTest, ConnectHappyPath) {
  mqtt_connect("my-device-uuid");

  EXPECT_EQ(MqttMockState::instance().last_connect_id,   "my-device-uuid");
  EXPECT_EQ(MqttMockState::instance().last_connect_user, MQTT_USERNAME);
  EXPECT_EQ(MqttMockState::instance().last_connect_pass, MQTT_PASSWORD);
  EXPECT_EQ(MqttMockState::instance().last_subscribe_topic, "devices/my-device-uuid/values");
}

// ===========================================================================
// mqtt_try_connect_once — thermostat-specific single-attempt variant
// ===========================================================================

TEST_F(MqttHandlerTest, TryConnectOnceReturnsTrueAndSubscribesOnSuccess) {
  MqttMockState::instance().connect_result = true;

  bool result = mqtt_try_connect_once("my-device-uuid");

  EXPECT_TRUE(result);
  EXPECT_EQ(MqttMockState::instance().last_connect_id, "my-device-uuid");
  EXPECT_EQ(MqttMockState::instance().last_connect_user, MQTT_USERNAME);
  EXPECT_EQ(MqttMockState::instance().last_subscribe_topic, "devices/my-device-uuid/values");
}

TEST_F(MqttHandlerTest, TryConnectOnceReturnsFalseOnFailure) {
  MqttMockState::instance().connect_result = false;

  bool result = mqtt_try_connect_once("my-device-uuid");

  EXPECT_FALSE(result);
  // No subscription should be attempted on failure.
  EXPECT_EQ(MqttMockState::instance().last_subscribe_topic, "");
}
