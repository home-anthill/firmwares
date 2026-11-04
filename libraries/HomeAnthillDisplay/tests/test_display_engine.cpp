#include <gtest/gtest.h>
#include <HomeAnthillDisplay.h>
#include <HomeAnthillCore.h>
#include "Adafruit_SSD1306.h"

namespace {
TwoWire display_bus;

ha::DisplayConfig test_config() {
  ha::DisplayConfig config;
  config.wire = &display_bus;
  config.sda = 4;
  config.scl = 5;
  config.button_pin = 42;
  return config;
}
}

class DisplayEngineTest : public ::testing::Test {
protected:
  void SetUp() override {
    DisplayMockState::reset();
    GpioMockState::reset();
    EspMockState::reset();
    Wire = TwoWire{};
    display_bus = TwoWire{};
    g_digital_read_value = HIGH;
    mock_set_millis(0);
    ha::display_reset_for_test(test_config());
    feature_values_clear();
  }
};

TEST_F(DisplayEngineTest, DisabledDisplayDoesNotTouchHardwareOrRestart) {
  auto config = test_config();
  config.enabled = false;
  ha::display_init(config);
  ha::display_set_connectivity_status(false, false);
  ha::display_show_message("title", "detail");
  ha::display_set_overlay("a", "b", "c");
  ha::display_sleep_for(1000);
  ha::display_force_update();
  g_digital_read_value = LOW;
  mock_advance_millis(20000);
  ha::display_update();
  EXPECT_EQ(DisplayMockState::instance().begin_count, 0);
  EXPECT_EQ(DisplayMockState::instance().display_count, 0);
  EXPECT_TRUE(GpioMockState::instance().pin_modes.empty());
  EXPECT_EQ(display_bus.sda, -1);
  EXPECT_EQ(EspMockState::instance().restart_count, 0);
  EXPECT_FALSE(ha::display_is_available_for_test());
}

TEST_F(DisplayEngineTest, ReinitializationClearsOverlayAndRestartsWakeDeadline) {
  auto config = test_config();
  JsonDocument doc;
  JsonArray features = doc.to<JsonArray>();
  features.add<JsonObject>()["name"] = "temperature";
  ha::feature_values_init(features, false);
  feature_values_set("temperature", 1);
  ha::display_init(config);
  ha::display_set_overlay("calibration", "line 2", "line 3");

  mock_set_millis(10000);
  ha::display_init(config);
  feature_values_set("temperature", 2);
  ha::display_force_update();
  EXPECT_EQ(DisplayMockState::instance().prints.back(), "2.0");
  EXPECT_EQ(DisplayMockState::instance().begin_count, 2);
  mock_set_millis(39999);
  ha::display_update();
  EXPECT_TRUE(ha::display_is_powered_for_test());
  mock_set_millis(40000);
  ha::display_update();
  EXPECT_FALSE(ha::display_is_powered_for_test());
}

TEST_F(DisplayEngineTest, UsesConfiguredBusAndLeavesDisabledButtonUntouched) {
  auto config = test_config();
  config.button_enabled = false;
  ha::display_init(config);
  EXPECT_EQ(display_bus.sda, 4);
  EXPECT_EQ(display_bus.scl, 5);
  EXPECT_EQ(Wire.sda, -1);
  EXPECT_TRUE(GpioMockState::instance().pin_modes.empty());
  EXPECT_TRUE(ha::display_is_available_for_test());
}

TEST_F(DisplayEngineTest, RejectsChangingDriverWiringAfterInitialization) {
  auto config = test_config();
  ha::display_init(config);
  int begin_count = DisplayMockState::instance().begin_count;
  GpioMockState::reset();

  TwoWire other_bus;
  config.wire = &other_bus;
  ha::display_init(config);
  EXPECT_FALSE(ha::display_is_available_for_test());
  EXPECT_EQ(other_bus.sda, -1);
  EXPECT_TRUE(GpioMockState::instance().pin_modes.empty());
  EXPECT_EQ(DisplayMockState::instance().begin_count, begin_count);

  config = test_config();
  config.reset_pin = 7;
  ha::display_init(config);
  EXPECT_FALSE(ha::display_is_available_for_test());
  EXPECT_EQ(DisplayMockState::instance().begin_count, begin_count);
}

TEST_F(DisplayEngineTest, MissingBusFailsWithoutHardwareAccess) {
  auto config = test_config();
  config.wire = nullptr;
  ha::display_init(config);
  ha::display_update();
  EXPECT_FALSE(ha::display_is_available_for_test());
  EXPECT_EQ(DisplayMockState::instance().begin_count, 0);
  EXPECT_TRUE(GpioMockState::instance().pin_modes.empty());
}
