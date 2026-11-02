#include <gtest/gtest.h>
#include <HomeAnthillCore.h>
#include <Arduino.h>
#include <HTTPClient.h>

TEST(CoreConfiguration, MqttAuthenticationComesFromEachCall) {
  ha::MqttConfig config{"broker.example", 1883, true, "first", "password", "key"};
  MqttMockState::reset();
  EXPECT_TRUE(ha::mqtt_try_connect_once("device", config));
  EXPECT_EQ(MqttMockState::instance().last_connect_user, "first");
  EXPECT_EQ(MqttMockState::instance().last_subscribe_topic, "devices/device/values");

  MqttMockState::reset();
  config.username = "second";
  EXPECT_TRUE(ha::mqtt_try_connect_once("device", config));
  EXPECT_EQ(MqttMockState::instance().last_connect_user, "second");

  MqttMockState::reset();
  config.authenticate = false;
  EXPECT_TRUE(ha::mqtt_try_connect_once("device", config));
  EXPECT_TRUE(MqttMockState::instance().last_connect_user.empty());
}

TEST(CoreConfiguration, SingleMqttFailureReturnsWithoutRestart) {
  MqttMockState::reset();
  EspMockState::reset();
  MqttMockState::instance().connect_result = false;
  ha::MqttConfig config{"broker.example", 1883, false, nullptr, nullptr, "key"};
  EXPECT_FALSE(ha::mqtt_try_connect_once("device", config));
  EXPECT_EQ(EspMockState::instance().restart_count, 0);
}

TEST(CoreConfiguration, RegistrationUsesSuppliedIdentityEndpointAndTls) {
  HttpMockConfig::reset();
  HttpMockConfig::instance().response_code = HTTP_CODE_CONFLICT;
  ha::RegistrationConfig config{"maker", "model", "key", "admission.example", 443, "/register", true};
  WiFiClient client;
  JsonDocument features;
  features.to<JsonArray>();
  EXPECT_EQ(ha::register_server(client, "aa:bb", features, config, false), 0);
  EXPECT_EQ(HttpMockConfig::instance().last_url, "https://admission.example:443/register");
  JsonDocument payload;
  ASSERT_FALSE(deserializeJson(payload, HttpMockConfig::instance().last_payload));
  EXPECT_STREQ(payload["manufacturer"], "maker");
  EXPECT_STREQ(payload["model"], "model");
  EXPECT_STREQ(payload["apiToken"], "key");

  config.tls = false;
  config.port = 80;
  EXPECT_EQ(ha::register_server(client, "aa:bb", features, config, false), 0);
  EXPECT_EQ(HttpMockConfig::instance().last_url, "http://admission.example:80/register");
}

TEST(CoreConfiguration, SingleRegistrationNetworkFailureReturnsWithoutRestart) {
  HttpMockConfig::reset();
  EspMockState::reset();
  HttpMockConfig::instance().response_code = -1;
  ha::RegistrationConfig config{"maker", "model", "key", "admission.example", 80, "/register", false};
  WiFiClient client;
  JsonDocument features;
  EXPECT_EQ(ha::register_server(client, "aa:bb", features, config, false), 1);
  EXPECT_EQ(EspMockState::instance().restart_count, 0);
}

TEST(CoreConfiguration, WifiUsesSuppliedCredentialsAndPreservesMacBufferBoundary) {
  WiFi = WifiMock{};
  ha::wifi_start_connect({"test-network", "test-password"});
  EXPECT_EQ(WiFi.ssid, "test-network");
  EXPECT_EQ(WiFi.password, "test-password");
  EXPECT_EQ(WiFi.begin_count, 1);
  char mac[19] = {};
  mac[18] = '!';
  ha::wifi_populate_mac(mac);
  EXPECT_STREQ(mac, "aa:bb:cc:dd:ee:ff");
  EXPECT_EQ(mac[18], '!');
}
