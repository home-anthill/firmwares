#include <gtest/gtest.h>
#include <cstring>
#include <string>

// Mock headers must precede production source includes.
#include "WiFi.h"
#include "PubSubClient.h"

#include <ArduinoJson.h>
#include <HomeAnthillCore.h>
#include "config.h"
#include "secrets.h"  // gives access to API_TOKEN, MQTT_USERNAME, MQTT_PASSWORD
#include <HomeAnthillConfig.h>

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
  ha::mqtt_notify_value("dev-uuid", "feat-uuid", "temperature", 23.5f, mqtt_config);
  EXPECT_EQ(MqttMockState::instance().last_publish_topic, "sensors/dev-uuid/temperature");

  ha::mqtt_notify_value("dev-uuid", "feat-uuid", "humidity", 60.0f, mqtt_config);
  EXPECT_EQ(MqttMockState::instance().last_publish_topic, "sensors/dev-uuid/humidity");
}

TEST_F(MqttHandlerTest, NotifyIndexValuesUseKebabCaseTopics) {
  ha::mqtt_notify_value("dev-uuid", "feat-uuid", "vocIndex", 123.4f, mqtt_config, false, "voc-index");
  EXPECT_EQ(MqttMockState::instance().last_publish_topic, "sensors/dev-uuid/voc-index");
  ha::mqtt_notify_value("dev-uuid", "feat-uuid", "noxIndex", 234.5f, mqtt_config, false, "nox-index");
  EXPECT_EQ(MqttMockState::instance().last_publish_topic, "sensors/dev-uuid/nox-index");
}

TEST_F(MqttHandlerTest, NotifyValueUsesOnlineTopicForOnlineType) {
  ha::mqtt_notify_value("dev-uuid", "feat-uuid", "online", 1.0f, mqtt_config);
  EXPECT_EQ(MqttMockState::instance().last_publish_topic,
            "online/dev-uuid/features/feat-uuid");
}

// ===========================================================================
// mqtt_notify_value — payload structure
// ===========================================================================

TEST_F(MqttHandlerTest, NotifyValuePayloadStructure) {
  ha::mqtt_notify_value("dev-uuid", "feature-uuid-456", "temperature", 23.5f, mqtt_config);

  JsonDocument doc;
  deserializeJson(doc, MqttMockState::instance().last_publish_payload);
  EXPECT_FALSE(doc["apiToken"].is<const char*>());
  EXPECT_STREQ(doc["deviceUuid"].as<const char*>(), "dev-uuid");
  EXPECT_STREQ(doc["featureUuid"].as<const char*>(), "feature-uuid-456");
  EXPECT_GT(doc["timestamp"].as<long>(), 0);
  EXPECT_STREQ(doc["nonce"].as<const char*>(), "000102030405060708090a0b0c0d0e0f");
  EXPECT_STREQ(doc["signature"].as<const char*>(), "aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899");
  EXPECT_FLOAT_EQ(doc["payload"]["value"].as<float>(), 23.5f);
}

TEST_F(MqttHandlerTest, NotifyValuePayloadEncodesNegativeValue) {
  ha::mqtt_notify_value("dev-uuid", "feat-uuid", "temperature", -5.0f, mqtt_config);

  JsonDocument doc;
  deserializeJson(doc, MqttMockState::instance().last_publish_payload);
  EXPECT_FLOAT_EQ(doc["payload"]["value"].as<float>(), -5.0f);
}

TEST_F(MqttHandlerTest, NotifyValuePayloadKeepsFloatIntegerDecimalForRustCanonicalForm) {
  ha::mqtt_notify_value("dev-uuid", "feat-uuid", "temperature", 22.0f, mqtt_config);

  EXPECT_NE(MqttMockState::instance().last_publish_payload.find(R"("payload":{"value":22.0})"),
            std::string::npos);
}

TEST_F(MqttHandlerTest, NotifyValuePayloadTrimsUnneededTrailingDecimalZeros) {
  ha::mqtt_notify_value("dev-uuid", "feat-uuid", "temperature", 22.2500f, mqtt_config);

  EXPECT_NE(MqttMockState::instance().last_publish_payload.find(R"("payload":{"value":22.25})"),
            std::string::npos);
}

// ===========================================================================
// mqtt_notify_value — disconnect on failure
// ===========================================================================

TEST_F(MqttHandlerTest, NotifyValueDisconnectsOnPublishFailure) {
  MqttMockState::instance().publish_result = false;
  ha::mqtt_notify_value("dev-uuid", "feat-uuid", "temperature", 23.5f, mqtt_config);
  EXPECT_EQ(MqttMockState::instance().disconnect_count, 1);
}

TEST_F(MqttHandlerTest, NotifyValueDoesNotDisconnectOnPublishSuccess) {
  MqttMockState::instance().publish_result = true;
  ha::mqtt_notify_value("dev-uuid", "feat-uuid", "temperature", 23.5f, mqtt_config);
  EXPECT_EQ(MqttMockState::instance().disconnect_count, 0);
}

// ===========================================================================
// mqtt_connect — happy path
// secrets.h has MQTT_AUTH true, so connect(id, user, pass) is used.
// ===========================================================================

TEST_F(MqttHandlerTest, ConnectHappyPath) {
  ha::mqtt_connect("my-device-uuid", mqtt_config);

  EXPECT_EQ(MqttMockState::instance().last_connect_id, "my-device-uuid");
  EXPECT_EQ(MqttMockState::instance().last_connect_user, MQTT_USERNAME);
  EXPECT_EQ(MqttMockState::instance().last_connect_pass, MQTT_PASSWORD);
  EXPECT_EQ(MqttMockState::instance().last_subscribe_topic, "devices/my-device-uuid/values");
}
