#include <gtest/gtest.h>
#include <cstring>

// Mock headers first so production includes resolve to stubs.
#include <Arduino.h>
#include "Wire.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"

#include "display.h"
#include "feature_values.h"

static const uint8_t kDisplayButtonPin = 42;

static FeatureValue g_feature_values[3];
static size_t g_feature_values_count = 0;

void feature_values_init(JsonArray /*features*/) {}
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
    display_reset_for_test();
    display_set_button_enabled_for_test(true);
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
    init_display(kDisplayButtonPin);
    clearDisplayMockCalls();
    display_invalidate_render_for_test();
  }

  void pressButtonFor(unsigned long duration_ms) {
    g_digital_read_value = LOW;
    update_display();
    mock_advance_millis(50);
    update_display();
    mock_advance_millis(duration_ms);
    g_digital_read_value = HIGH;
    update_display();
    mock_advance_millis(50);
    update_display();
  }
};

TEST_F(DisplayTest, InitDisplayCallsBeginOnce) {
  init_display(kDisplayButtonPin);

  EXPECT_EQ(DisplayMockState::instance().begin_count, 1);
}

TEST_F(DisplayTest, InitDisplayConfiguresButtonPinPullup) {
  init_display(kDisplayButtonPin);

  EXPECT_EQ(GpioMockState::instance().pin_modes[kDisplayButtonPin], INPUT_PULLUP);
}

TEST_F(DisplayTest, InitDisplayDoesNotConfigureButtonWhenDisabled) {
  display_set_button_enabled_for_test(false);

  init_display(kDisplayButtonPin);

  EXPECT_TRUE(GpioMockState::instance().pin_modes.empty());
  EXPECT_EQ(display_button_pin_for_test(), 255);
}

TEST_F(DisplayTest, InitDisplayClearsAndPrintsStarting) {
  init_display(kDisplayButtonPin);

  EXPECT_GE(DisplayMockState::instance().clear_count, 1);
  EXPECT_GE(DisplayMockState::instance().display_count, 1);
  EXPECT_STREQ(DisplayMockState::instance().last_print_str, "Starting");
}

TEST_F(DisplayTest, InitDisplayLeavesShowDisplayTrueOnSuccess) {
  DisplayMockState::instance().begin_result = true;

  init_display(kDisplayButtonPin);

  EXPECT_TRUE(display_is_available_for_test());
}

TEST_F(DisplayTest, InitDisplaySetsShowDisplayFalseOnBeginFailure) {
  DisplayMockState::instance().begin_result = false;

  init_display(kDisplayButtonPin);

  EXPECT_FALSE(display_is_available_for_test());
}

TEST_F(DisplayTest, UpdateDisplayDoesNothingWhenNoFeatureValuesExist) {
  initDisplayForUpdate();

  update_display();

  EXPECT_EQ(DisplayMockState::instance().display_count, 0);
  EXPECT_EQ(DisplayMockState::instance().clear_count, 0);
}

TEST_F(DisplayTest, UpdateDisplayDoesNothingWhenNoFeatureHasValueYet) {
  initDisplayForUpdate();
  addFeatureValue(0, "temperature", "°C", 0.0f, false);
  addFeatureValue(1, "humidity", "%", 0.0f, false);

  update_display();

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
  EXPECT_EQ(DisplayMockState::instance().prints[1], std::string("23.50 \xF8""C"));
}

TEST_F(DisplayTest, UpdateDisplayRefreshesVisibleFeatureValue) {
  initDisplayForUpdate();
  addFeatureValue(0, "temperature", "°C", 23.4f, true);
  pressButtonFor(100);
  clearDisplayMockCalls();

  g_feature_values[0].value = 23.5f;
  update_display();

  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[1], std::string("23.50 \xF8""C"));
}

TEST_F(DisplayTest, DisabledButtonRefreshesVisibleTemperatureValue) {
  display_set_button_enabled_for_test(false);
  initDisplayForUpdate();
  addFeatureValue(0, "setpoint", "°C", 22.0f, true);
  addFeatureValue(1, "temperature", "°C", 23.4f, true);
  update_display();
  clearDisplayMockCalls();

  g_feature_values[1].value = 23.5f;
  update_display();

  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[1], std::string("23.50 \xF8""C"));
}

TEST_F(DisplayTest, ButtonPressSkipsFeaturesWithoutValues) {
  initDisplayForUpdate();
  addFeatureValue(0, "temperature", "°C", 0.0f, false);
  addFeatureValue(1, "setpoint", "°C", 22.0f, true);

  pressButtonFor(100);

  EXPECT_EQ(DisplayMockState::instance().display_count, 0);
  EXPECT_EQ(DisplayMockState::instance().clear_count, 0);
}

TEST_F(DisplayTest, ButtonPressOmitsDashUnit) {
  initDisplayForUpdate();
  addFeatureValue(0, "temperature", "-", 23.5f, true);

  pressButtonFor(100);

  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[1], "23.5");
}

TEST_F(DisplayTest, ButtonPressSkipsOnlineFeatureValue) {
  initDisplayForUpdate();
  addFeatureValue(0, "online", "-", 1.0f, true);
  addFeatureValue(1, "temperature", "°C", 23.5f, true);

  pressButtonFor(100);

  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[1], std::string("23.50 \xF8""C"));
}

TEST_F(DisplayTest, ButtonPressShowsOnlyTemperatureAndWraps) {
  initDisplayForUpdate();
  addFeatureValue(0, "setpoint", "°C", 22.0f, true);
  addFeatureValue(1, "temperature", "°C", 23.5f, true);
  addFeatureValue(2, "mode", "", 3.0f, true);

  pressButtonFor(100);
  pressButtonFor(100);

  ASSERT_GE(DisplayMockState::instance().prints.size(), 4u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[1], std::string("23.50 \xF8""C"));
  EXPECT_EQ(DisplayMockState::instance().prints[2], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[3], std::string("23.50 \xF8""C"));
}

TEST_F(DisplayTest, UpdateDisplayDoesNothingWhenDisplayIsDisabled) {
  initDisplayForUpdate();
  display_set_available_for_test(false);
  addFeatureValue(0, "temperature", "°C", 23.5f, true);

  update_display();

  EXPECT_EQ(DisplayMockState::instance().display_count, 0);
  EXPECT_EQ(DisplayMockState::instance().clear_count, 0);
}

TEST_F(DisplayTest, StartupAutoPowersOffDisplayAfterThirtySeconds) {
  init_display(kDisplayButtonPin);
  clearDisplayMockCalls();

  mock_advance_millis(30000);
  update_display();

  EXPECT_FALSE(display_is_powered_for_test());
  ASSERT_EQ(DisplayMockState::instance().commands.size(), 1u);
  EXPECT_EQ(DisplayMockState::instance().commands[0], SSD1306_DISPLAYOFF);
}

TEST_F(DisplayTest, ConnectivityErrorShowsNoticeThenTemperatureAndStaysOn) {
  init_display(kDisplayButtonPin);
  addFeatureValue(0, "temperature", "°C", 23.5f, true);
  display_set_connectivity_status(false, false);
  clearDisplayMockCalls();

  update_display();

  EXPECT_TRUE(display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "WiFi status");
  EXPECT_EQ(DisplayMockState::instance().prints[1], "Offline");

  clearDisplayMockCalls();
  mock_advance_millis(9999);
  update_display();
  EXPECT_TRUE(DisplayMockState::instance().prints.empty());

  mock_advance_millis(1);
  update_display();

  EXPECT_TRUE(display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[1], std::string("23.50 \xF8""C"));
  EXPECT_TRUE(DisplayMockState::instance().commands.empty());
}

TEST_F(DisplayTest, ConnectivityNoticeDoesNotExtendForSameErrorStatus) {
  init_display(kDisplayButtonPin);
  addFeatureValue(0, "temperature", "°C", 23.5f, true);
  display_set_connectivity_status(true, false);
  update_display();
  clearDisplayMockCalls();

  mock_advance_millis(5000);
  display_set_connectivity_status(true, false);
  mock_advance_millis(5000);
  update_display();

  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[1], std::string("23.50 \xF8""C"));
}

TEST_F(DisplayTest, ConnectivityErrorOverridesCommandSleep) {
  init_display(kDisplayButtonPin);
  display_sleep_for(30000);
  display_set_connectivity_status(true, false);
  clearDisplayMockCalls();

  update_display();

  EXPECT_TRUE(display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "MQTT error");
  EXPECT_EQ(DisplayMockState::instance().prints[1], "Disconnected");
  ASSERT_EQ(DisplayMockState::instance().commands.size(), 1u);
  EXPECT_EQ(DisplayMockState::instance().commands[0], SSD1306_DISPLAYON);
}

TEST_F(DisplayTest, CommandSleepPowersDisplayOff) {
  init_display(kDisplayButtonPin);
  clearDisplayMockCalls();

  display_sleep_for(30000);

  EXPECT_FALSE(display_is_powered_for_test());
  ASSERT_EQ(DisplayMockState::instance().commands.size(), 1u);
  EXPECT_EQ(DisplayMockState::instance().commands[0], SSD1306_DISPLAYOFF);
}

TEST_F(DisplayTest, ForceUpdateDoesNotWakeDuringCommandSleep) {
  init_display(kDisplayButtonPin);
  addFeatureValue(0, "temperature", "°C", 23.5f, true);
  display_sleep_for(30000);
  clearDisplayMockCalls();

  display_force_update();

  EXPECT_FALSE(display_is_powered_for_test());
  EXPECT_TRUE(DisplayMockState::instance().commands.empty());
  EXPECT_TRUE(DisplayMockState::instance().prints.empty());
}

TEST_F(DisplayTest, SinglePressTurnsDisplayOnForThirtySecondsWhenOff) {
  init_display(kDisplayButtonPin);
  mock_advance_millis(30000);
  update_display();
  clearDisplayMockCalls();

  pressButtonFor(100);
  EXPECT_TRUE(display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().commands.size(), 1u);
  EXPECT_EQ(DisplayMockState::instance().commands[0], SSD1306_DISPLAYON);

  mock_advance_millis(29999);
  update_display();
  EXPECT_TRUE(display_is_powered_for_test());

  mock_advance_millis(1);
  update_display();
  EXPECT_FALSE(display_is_powered_for_test());
  EXPECT_EQ(DisplayMockState::instance().commands.back(), SSD1306_DISPLAYOFF);
}

TEST_F(DisplayTest, SinglePressWhileOnKeepsShowingTemperature) {
  init_display(kDisplayButtonPin);
  addFeatureValue(0, "temperature", "°C", 23.5f, true);
  addFeatureValue(1, "setpoint", "°C", 22.0f, true);
  clearDisplayMockCalls();

  pressButtonFor(100);
  pressButtonFor(100);

  EXPECT_TRUE(display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().prints.size(), 4u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[2], "temperature");
}

TEST_F(DisplayTest, SinglePressSkipsConnectivityErrorAndShowsFeatureValue) {
  init_display(kDisplayButtonPin);
  addFeatureValue(0, "temperature", "°C", 23.5f, true);
  display_set_connectivity_status(false, false);
  update_display();
  clearDisplayMockCalls();

  pressButtonFor(100);

  EXPECT_TRUE(display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[1], std::string("23.50 \xF8""C"));
}

TEST_F(DisplayTest, ConnectivityErrorDoesNotReturnAfterNoticeExpires) {
  init_display(kDisplayButtonPin);
  addFeatureValue(0, "temperature", "°C", 23.5f, true);
  display_set_connectivity_status(false, false);
  update_display();

  mock_advance_millis(10000);
  update_display();
  clearDisplayMockCalls();

  mock_advance_millis(30000);
  g_feature_values[0].value = 23.6f;
  update_display();

  EXPECT_TRUE(display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[1], std::string("23.60 \xF8""C"));
}

TEST_F(DisplayTest, DisabledButtonKeepsDisplayOnPastStartupTimeout) {
  display_set_button_enabled_for_test(false);
  init_display(kDisplayButtonPin);
  clearDisplayMockCalls();

  mock_advance_millis(60000);
  update_display();

  EXPECT_TRUE(display_is_powered_for_test());
  EXPECT_TRUE(DisplayMockState::instance().commands.empty());
}

TEST_F(DisplayTest, DisabledButtonIgnoresCommandSleep) {
  display_set_button_enabled_for_test(false);
  init_display(kDisplayButtonPin);
  clearDisplayMockCalls();

  display_sleep_for(30000);
  mock_advance_millis(30000);
  update_display();

  EXPECT_TRUE(display_is_powered_for_test());
  EXPECT_TRUE(DisplayMockState::instance().commands.empty());
}

TEST_F(DisplayTest, DisabledButtonAllowsForceUpdateAfterCommandSleep) {
  display_set_button_enabled_for_test(false);
  init_display(kDisplayButtonPin);
  addFeatureValue(0, "temperature", "°C", 23.5f, true);
  display_sleep_for(30000);
  clearDisplayMockCalls();

  display_force_update();

  EXPECT_TRUE(display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
}

TEST_F(DisplayTest, DisabledButtonPrefersTemperatureOverControllerValues) {
  display_set_button_enabled_for_test(false);
  init_display(kDisplayButtonPin);
  addFeatureValue(0, "setpoint", "°C", 22.0f, true);
  addFeatureValue(1, "tolerance", "°C", 1.0f, true);
  addFeatureValue(2, "temperature", "°C", 23.5f, true);
  clearDisplayMockCalls();

  update_display();

  EXPECT_TRUE(display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[1], std::string("23.50 \xF8""C"));
}

TEST_F(DisplayTest, DisabledButtonCommandMessageStaysVisibleThenReturnsToTemperature) {
  display_set_button_enabled_for_test(false);
  init_display(kDisplayButtonPin);
  addFeatureValue(0, "setpoint", "°C", 22.0f, true);
  addFeatureValue(1, "tolerance", "°C", 1.0f, true);
  addFeatureValue(2, "temperature", "°C", 23.5f, true);
  clearDisplayMockCalls();

  display_show_message("Command", "Received");

  EXPECT_TRUE(display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "Command");
  EXPECT_EQ(DisplayMockState::instance().prints[1], "Received");

  clearDisplayMockCalls();
  mock_advance_millis(1499);
  update_display();
  EXPECT_TRUE(DisplayMockState::instance().prints.empty());

  mock_advance_millis(1);
  update_display();
  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[1], std::string("23.50 \xF8""C"));
}

TEST_F(DisplayTest, DisabledButtonIgnoresSinglePress) {
  display_set_button_enabled_for_test(false);
  init_display(kDisplayButtonPin);
  clearDisplayMockCalls();

  pressButtonFor(100);

  EXPECT_TRUE(display_is_powered_for_test());
  EXPECT_TRUE(DisplayMockState::instance().commands.empty());
}

TEST_F(DisplayTest, DisabledButtonIgnoresLongPress) {
  display_set_button_enabled_for_test(false);
  init_display(kDisplayButtonPin);

  g_digital_read_value = LOW;
  update_display();
  mock_advance_millis(50);
  update_display();
  mock_advance_millis(30000);
  update_display();

  EXPECT_EQ(EspMockState::instance().restart_count, 0);
  EXPECT_TRUE(display_is_powered_for_test());
}

TEST_F(DisplayTest, LongPressRestartsEsp32AfterTenSeconds) {
  init_display(kDisplayButtonPin);

  g_digital_read_value = LOW;
  update_display();
  mock_advance_millis(50);
  update_display();
  mock_advance_millis(9999);
  update_display();

  EXPECT_EQ(EspMockState::instance().restart_count, 0);

  mock_advance_millis(1);
  update_display();
  EXPECT_EQ(EspMockState::instance().restart_count, 1);

  g_digital_read_value = HIGH;
  mock_advance_millis(50);
  update_display();
  EXPECT_EQ(EspMockState::instance().restart_count, 1);
}

TEST_F(DisplayTest, ShowMessagePowersDisplayAndKeepsItOnTemporarily) {
  init_display(kDisplayButtonPin);
  display_sleep_for(30000);
  clearDisplayMockCalls();

  display_show_message("Command", "Received");

  EXPECT_TRUE(display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "Command");
  EXPECT_EQ(DisplayMockState::instance().prints[1], "Received");

  mock_advance_millis(29999);
  update_display();
  EXPECT_TRUE(display_is_powered_for_test());

  mock_advance_millis(1);
  update_display();
  EXPECT_FALSE(display_is_powered_for_test());
}

TEST_F(DisplayTest, ShowMessageCanRenderWhileConnectivityIsPending) {
  init_display(kDisplayButtonPin);
  display_set_connectivity_status(false, false);
  clearDisplayMockCalls();

  display_show_message("Command", "Received");

  EXPECT_TRUE(display_is_powered_for_test());
  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "Command");
  EXPECT_EQ(DisplayMockState::instance().prints[1], "Received");
}
