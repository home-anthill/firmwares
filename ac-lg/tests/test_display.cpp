#include "config.h"
#include "secrets.h"

#include <gtest/gtest.h>
#include <cstring>

// Mock headers first so production includes resolve to stubs.
#include <Arduino.h>
#include "Wire.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"

#include <HomeAnthillDisplay.h>
#include <HomeAnthillDisplayConfig.h>
#include "display_policy.h"
#include <HomeAnthillCore.h>

static const ha::DisplayConfig kInitialDisplayConfig = display_config;

static const uint8_t kDisplayButtonPin = DISPLAY_BUTTON_PIN;

static FeatureValue g_feature_values[3];
static size_t g_feature_values_count = 0;

void ha::feature_values_init(JsonArray /*features*/, bool /*controller_defaults*/) {}
void feature_values_clear() {}

size_t feature_values_count() {
  return g_feature_values_count;
}

bool feature_values_set(const char* /*name*/, float /*value*/) {
  return false;
}

bool feature_values_get(size_t index, FeatureValue* value) {
  if (value == nullptr || index >= g_feature_values_count) {
    return false;
  }

  *value = g_feature_values[index];
  return true;
}

class DisplayTest : public ::testing::Test {
protected:
  void SetUp() override {
    DisplayMockState::reset();
    GpioMockState::reset();
    EspMockState::reset();
    g_digital_read_value = HIGH;
    display_config = kInitialDisplayConfig;
    ha::display_reset_for_test(display_config, display_policy);
    display_config.button_enabled = true;
    mock_set_millis(0);
    memset(g_feature_values, 0, sizeof(g_feature_values));
    g_feature_values_count = 0;
  }

  void addFeatureValue(size_t index, const char* name, const char* unit, float value, bool has_value) {
    ASSERT_LT(index, 3u);
    strncpy(g_feature_values[index].name, name, sizeof(g_feature_values[index].name) - 1);
    strncpy(g_feature_values[index].unit, unit, sizeof(g_feature_values[index].unit) - 1);
    g_feature_values[index].value = value;
    g_feature_values[index].has_value = has_value;
    if (g_feature_values_count <= index) {
      g_feature_values_count = index + 1;
    }
  }

  void clearDisplayMockCalls() {
    auto& state = DisplayMockState::instance();
    state.clear_count = 0;
    state.display_count = 0;
    state.command_count = 0;
    state.last_command = 0;
    state.last_print_str = nullptr;
    state.prints.clear();
    state.commands.clear();
  }

  void initDisplayForUpdate() {
    ha::display_init(display_config, display_policy);
    clearDisplayMockCalls();
    ha::display_invalidate_render_for_test();
  }

  void pressButtonFor(unsigned long duration_ms) {
    g_digital_read_value = LOW;
    ha::display_update();
    mock_advance_millis(50);
    ha::display_update();
    mock_advance_millis(duration_ms);
    g_digital_read_value = HIGH;
    ha::display_update();
    mock_advance_millis(50);
    ha::display_update();
  }
};

TEST_F(DisplayTest, InitDisplayCallsBeginOnce) {
  ha::display_init(display_config, display_policy);

  EXPECT_EQ(DisplayMockState::instance().begin_count, 1);
}

TEST_F(DisplayTest, InitDisplayConfiguresButtonPinPullup) {
  ha::display_init(display_config, display_policy);

  EXPECT_EQ(GpioMockState::instance().pin_modes[kDisplayButtonPin], INPUT_PULLUP);
}

TEST_F(DisplayTest, InitDisplayDoesNotConfigureButtonWhenDisabled) {
  display_config.button_enabled = false;

  ha::display_init(display_config, display_policy);

  EXPECT_TRUE(GpioMockState::instance().pin_modes.empty());
  EXPECT_EQ(ha::display_button_pin_for_test(), 255);
}

TEST_F(DisplayTest, InitDisplayClearsAndPrintsStarting) {
  ha::display_init(display_config, display_policy);

  EXPECT_GE(DisplayMockState::instance().clear_count, 1);
  EXPECT_GE(DisplayMockState::instance().display_count, 1);
  EXPECT_STREQ(DisplayMockState::instance().last_print_str, "Starting");
}

TEST_F(DisplayTest, InitDisplayLeavesShowDisplayTrueOnSuccess) {
  DisplayMockState::instance().begin_result = true;

  ha::display_init(display_config, display_policy);

  EXPECT_TRUE(ha::display_is_available_for_test());
}

TEST_F(DisplayTest, InitDisplaySetsShowDisplayFalseOnBeginFailure) {
  DisplayMockState::instance().begin_result = false;

  ha::display_init(display_config, display_policy);

  EXPECT_FALSE(ha::display_is_available_for_test());
}

TEST_F(DisplayTest, UpdateDisplayDoesNothingWhenNoFeatureValuesExist) {
  initDisplayForUpdate();

  ha::display_update();

  EXPECT_EQ(DisplayMockState::instance().display_count, 0);
  EXPECT_EQ(DisplayMockState::instance().clear_count, 0);
}

TEST_F(DisplayTest, UpdateDisplayDoesNothingWhenNoFeatureHasValueYet) {
  initDisplayForUpdate();
  addFeatureValue(0, "temperature", "°C", 0.0f, false);
  addFeatureValue(1, "humidity", "%", 0.0f, false);

  ha::display_update();

  EXPECT_EQ(DisplayMockState::instance().display_count, 0);
  EXPECT_EQ(DisplayMockState::instance().clear_count, 0);
}

TEST_F(DisplayTest, ButtonPressRendersFirstAvailableFeatureValue) {
  initDisplayForUpdate();
  addFeatureValue(0, "temperature", "°C", 23.5f, true);

  pressButtonFor(100);

  EXPECT_EQ(DisplayMockState::instance().clear_count, 1);
  EXPECT_EQ(DisplayMockState::instance().display_count, 1);
  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[1], std::string("23.5 \xF8""C"));
}

TEST_F(DisplayTest, UpdateDisplayRefreshesVisibleFeatureValue) {
  initDisplayForUpdate();
  addFeatureValue(0, "temperature", "°C", 23.4f, true);
  pressButtonFor(100);
  clearDisplayMockCalls();

  g_feature_values[0].value = 23.5f;
  ha::display_update();

  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[1], std::string("23.5 \xF8""C"));
}

TEST_F(DisplayTest, ButtonPressSkipsFeaturesWithoutValues) {
  initDisplayForUpdate();
  addFeatureValue(0, "temperature", "°C", 0.0f, false);
  addFeatureValue(1, "humidity", "%", 60.0f, true);

  pressButtonFor(100);

  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "humidity");
  EXPECT_EQ(DisplayMockState::instance().prints[1], "60.0 %");
}

TEST_F(DisplayTest, ButtonPressOmitsDashUnit) {
  initDisplayForUpdate();
  addFeatureValue(0, "status", "-", 1.0f, true);

  pressButtonFor(100);

  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "status");
  EXPECT_EQ(DisplayMockState::instance().prints[1], "1.0");
}

TEST_F(DisplayTest, ButtonPressSkipsOnlineFeatureValue) {
  initDisplayForUpdate();
  addFeatureValue(0, "online", "-", 1.0f, true);
  addFeatureValue(1, "temperature", "°C", 23.5f, true);

  pressButtonFor(100);

  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[1], std::string("23.5 \xF8""C"));
}

TEST_F(DisplayTest, ButtonPressAdvancesThroughAvailableFeatureValuesAndWraps) {
  initDisplayForUpdate();
  addFeatureValue(0, "temperature", "°C", 23.5f, true);
  addFeatureValue(1, "humidity", "%", 60.0f, true);

  pressButtonFor(100);
  pressButtonFor(100);
  pressButtonFor(100);

  ASSERT_GE(DisplayMockState::instance().prints.size(), 6u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[1], std::string("23.5 \xF8""C"));
  EXPECT_EQ(DisplayMockState::instance().prints[2], "humidity");
  EXPECT_EQ(DisplayMockState::instance().prints[3], "60.0 %");
  EXPECT_EQ(DisplayMockState::instance().prints[4], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[5], std::string("23.5 \xF8""C"));
}

TEST_F(DisplayTest, ButtonPressRotatesThroughControllerFeatureValuesAndWraps) {
  initDisplayForUpdate();
  addFeatureValue(0, "on", "", 0.0f, true);
  addFeatureValue(1, "setpoint", "°C", 22.0f, true);
  addFeatureValue(2, "mode", "", 4.0f, true);

  pressButtonFor(100);
  pressButtonFor(100);
  pressButtonFor(100);
  pressButtonFor(100);

  ASSERT_GE(DisplayMockState::instance().prints.size(), 8u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "on");
  EXPECT_EQ(DisplayMockState::instance().prints[1], "Off");
  EXPECT_EQ(DisplayMockState::instance().prints[2], "setpoint");
  EXPECT_EQ(DisplayMockState::instance().prints[3], std::string("22.0 \xF8""C"));
  EXPECT_EQ(DisplayMockState::instance().prints[4], "mode");
  EXPECT_EQ(DisplayMockState::instance().prints[5], "Heat");
  EXPECT_EQ(DisplayMockState::instance().prints[6], "on");
  EXPECT_EQ(DisplayMockState::instance().prints[7], "Off");
}

TEST_F(DisplayTest, ButtonPressMapsLgFanSpeedValuesToLabels) {
  initDisplayForUpdate();
  addFeatureValue(0, "fanSpeed", "-", 10.0f, true);
  addFeatureValue(1, "fanSpeed", "-", 2.0f, true);
  addFeatureValue(2, "fanSpeed", "-", 5.0f, true);

  pressButtonFor(100);
  pressButtonFor(100);
  pressButtonFor(100);

  ASSERT_GE(DisplayMockState::instance().prints.size(), 6u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "fanSpeed");
  EXPECT_EQ(DisplayMockState::instance().prints[1], "Max");
  EXPECT_EQ(DisplayMockState::instance().prints[2], "fanSpeed");
  EXPECT_EQ(DisplayMockState::instance().prints[3], "Med");
  EXPECT_EQ(DisplayMockState::instance().prints[4], "fanSpeed");
  EXPECT_EQ(DisplayMockState::instance().prints[5], "Auto");
}

TEST_F(DisplayTest, UpdateDisplayDoesNothingWhenDisplayIsDisabled) {
  initDisplayForUpdate();
  ha::display_set_available_for_test(false);
  addFeatureValue(0, "temperature", "°C", 23.5f, true);

  ha::display_update();

  EXPECT_EQ(DisplayMockState::instance().display_count, 0);
  EXPECT_EQ(DisplayMockState::instance().clear_count, 0);
}

TEST_F(DisplayTest, StartupAutoPowersOffDisplayAfterThirtySeconds) {
  ha::display_init(display_config, display_policy);
  clearDisplayMockCalls();

  mock_advance_millis(30000);
  ha::display_update();

  EXPECT_FALSE(ha::display_is_powered_for_test());
  ASSERT_EQ(DisplayMockState::instance().commands.size(), 1u);
  EXPECT_EQ(DisplayMockState::instance().commands[0], SSD1306_DISPLAYOFF);
}

TEST_F(DisplayTest, ConnectivityErrorPowersDisplayAndStaysOnPastAutoOff) {
  ha::display_init(display_config, display_policy);
  ha::display_set_connectivity_status(false, false);
  clearDisplayMockCalls();

  mock_advance_millis(30000);
  ha::display_update();
  mock_advance_millis(30000);
  ha::display_update();

  EXPECT_TRUE(ha::display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "WiFi status");
  EXPECT_EQ(DisplayMockState::instance().prints[1], "Offline");
  EXPECT_TRUE(DisplayMockState::instance().commands.empty());
}

TEST_F(DisplayTest, ConnectivityErrorRepaintsWhenErrorTypeChanges) {
  ha::display_init(display_config, display_policy);
  ha::display_set_connectivity_status(false, false);
  ha::display_update();
  clearDisplayMockCalls();

  ha::display_set_connectivity_status(true, false);
  ha::display_update();

  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "MQTT error");
  EXPECT_EQ(DisplayMockState::instance().prints[1], "Disconnected");
}

TEST_F(DisplayTest, ConnectivityErrorOverridesCommandSleep) {
  ha::display_init(display_config, display_policy);
  ha::display_sleep_for(30000);
  ha::display_set_connectivity_status(true, false);
  clearDisplayMockCalls();

  ha::display_update();

  EXPECT_TRUE(ha::display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "MQTT error");
  EXPECT_EQ(DisplayMockState::instance().prints[1], "Disconnected");
  ASSERT_EQ(DisplayMockState::instance().commands.size(), 1u);
  EXPECT_EQ(DisplayMockState::instance().commands[0], SSD1306_DISPLAYON);
}

TEST_F(DisplayTest, CommandSleepPowersDisplayOff) {
  ha::display_init(display_config, display_policy);
  clearDisplayMockCalls();

  ha::display_sleep_for(30000);

  EXPECT_FALSE(ha::display_is_powered_for_test());
  ASSERT_EQ(DisplayMockState::instance().commands.size(), 1u);
  EXPECT_EQ(DisplayMockState::instance().commands[0], SSD1306_DISPLAYOFF);
}

TEST_F(DisplayTest, ForceUpdateDoesNotWakeDuringCommandSleep) {
  ha::display_init(display_config, display_policy);
  addFeatureValue(0, "temperature", "°C", 23.5f, true);
  ha::display_sleep_for(30000);
  clearDisplayMockCalls();

  ha::display_force_update();

  EXPECT_FALSE(ha::display_is_powered_for_test());
  EXPECT_TRUE(DisplayMockState::instance().commands.empty());
  EXPECT_TRUE(DisplayMockState::instance().prints.empty());
}

TEST_F(DisplayTest, SinglePressTurnsDisplayOnForThirtySecondsWhenOff) {
  ha::display_init(display_config, display_policy);
  mock_advance_millis(30000);
  ha::display_update();
  clearDisplayMockCalls();

  pressButtonFor(100);
  EXPECT_TRUE(ha::display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().commands.size(), 1u);
  EXPECT_EQ(DisplayMockState::instance().commands[0], SSD1306_DISPLAYON);

  mock_advance_millis(29999);
  ha::display_update();
  EXPECT_TRUE(ha::display_is_powered_for_test());

  mock_advance_millis(1);
  ha::display_update();
  EXPECT_FALSE(ha::display_is_powered_for_test());
  EXPECT_EQ(DisplayMockState::instance().commands.back(), SSD1306_DISPLAYOFF);
}

TEST_F(DisplayTest, SinglePressWhileOnShowsNextFeature) {
  ha::display_init(display_config, display_policy);
  addFeatureValue(0, "temperature", "°C", 23.5f, true);
  addFeatureValue(1, "humidity", "%", 60.0f, true);
  clearDisplayMockCalls();

  pressButtonFor(100);
  pressButtonFor(100);

  EXPECT_TRUE(ha::display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().prints.size(), 4u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[2], "humidity");
}

TEST_F(DisplayTest, SinglePressSkipsConnectivityErrorAndShowsFeatureValue) {
  ha::display_init(display_config, display_policy);
  addFeatureValue(0, "temperature", "°C", 23.5f, true);
  ha::display_set_connectivity_status(false, false);
  ha::display_update();
  clearDisplayMockCalls();

  pressButtonFor(100);

  EXPECT_TRUE(ha::display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[1], std::string("23.5 \xF8""C"));
}

TEST_F(DisplayTest, ConnectivityErrorReturnsAfterFeatureInactivity) {
  ha::display_init(display_config, display_policy);
  addFeatureValue(0, "temperature", "°C", 23.5f, true);
  ha::display_set_connectivity_status(false, false);
  ha::display_update();
  clearDisplayMockCalls();

  pressButtonFor(100);
  mock_advance_millis(30000);
  ha::display_update();

  EXPECT_TRUE(ha::display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().prints.size(), 4u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[2], "WiFi status");
  EXPECT_EQ(DisplayMockState::instance().prints[3], "Offline");
}

TEST_F(DisplayTest, DisabledButtonKeepsDisplayOnPastStartupTimeout) {
  display_config.button_enabled = false;
  ha::display_init(display_config, display_policy);
  clearDisplayMockCalls();

  mock_advance_millis(60000);
  ha::display_update();

  EXPECT_TRUE(ha::display_is_powered_for_test());
  EXPECT_TRUE(DisplayMockState::instance().commands.empty());
}

TEST_F(DisplayTest, DisabledButtonIgnoresCommandSleep) {
  display_config.button_enabled = false;
  ha::display_init(display_config, display_policy);
  clearDisplayMockCalls();

  ha::display_sleep_for(30000);
  mock_advance_millis(30000);
  ha::display_update();

  EXPECT_TRUE(ha::display_is_powered_for_test());
  EXPECT_TRUE(DisplayMockState::instance().commands.empty());
}

TEST_F(DisplayTest, DisabledButtonAllowsForceUpdateAfterCommandSleep) {
  display_config.button_enabled = false;
  ha::display_init(display_config, display_policy);
  addFeatureValue(0, "temperature", "°C", 23.5f, true);
  ha::display_sleep_for(30000);
  clearDisplayMockCalls();

  ha::display_force_update();

  EXPECT_TRUE(ha::display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
}

TEST_F(DisplayTest, DisabledButtonIgnoresSinglePress) {
  display_config.button_enabled = false;
  ha::display_init(display_config, display_policy);
  clearDisplayMockCalls();

  pressButtonFor(100);

  EXPECT_TRUE(ha::display_is_powered_for_test());
  EXPECT_TRUE(DisplayMockState::instance().commands.empty());
}

TEST_F(DisplayTest, DisabledButtonIgnoresLongPress) {
  display_config.button_enabled = false;
  ha::display_init(display_config, display_policy);

  g_digital_read_value = LOW;
  ha::display_update();
  mock_advance_millis(50);
  ha::display_update();
  mock_advance_millis(30000);
  ha::display_update();

  EXPECT_EQ(EspMockState::instance().restart_count, 0);
  EXPECT_TRUE(ha::display_is_powered_for_test());
}

TEST_F(DisplayTest, LongPressRestartsEsp32AfterTenSeconds) {
  ha::display_init(display_config, display_policy);

  g_digital_read_value = LOW;
  ha::display_update();
  mock_advance_millis(50);
  ha::display_update();
  mock_advance_millis(9999);
  ha::display_update();

  EXPECT_EQ(EspMockState::instance().restart_count, 0);

  mock_advance_millis(1);
  ha::display_update();
  EXPECT_EQ(EspMockState::instance().restart_count, 1);

  g_digital_read_value = HIGH;
  mock_advance_millis(50);
  ha::display_update();
  EXPECT_EQ(EspMockState::instance().restart_count, 1);
}

TEST_F(DisplayTest, ShowMessagePowersDisplayAndKeepsItOnTemporarily) {
  ha::display_init(display_config, display_policy);
  ha::display_sleep_for(30000);
  clearDisplayMockCalls();

  ha::display_show_message("Command", "Received");

  EXPECT_TRUE(ha::display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "Command");
  EXPECT_EQ(DisplayMockState::instance().prints[1], "Received");

  mock_advance_millis(29999);
  ha::display_update();
  EXPECT_TRUE(ha::display_is_powered_for_test());

  mock_advance_millis(1);
  ha::display_update();
  EXPECT_FALSE(ha::display_is_powered_for_test());
}

TEST_F(DisplayTest, ShowMessageCanRenderWhileConnectivityIsPending) {
  ha::display_init(display_config, display_policy);
  ha::display_set_connectivity_status(false, false);
  clearDisplayMockCalls();

  ha::display_show_message("Command", "Received");

  EXPECT_TRUE(ha::display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "Command");
  EXPECT_EQ(DisplayMockState::instance().prints[1], "Received");
}
