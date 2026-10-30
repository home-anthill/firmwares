#include <gtest/gtest.h>

// Mock headers first so production includes resolve to stubs.
#include <Arduino.h>

#include "motion_sensor.h"
#include "ld2410.h"
#include "config.h"
#include "secrets.h"

// ---------------------------------------------------------------------------
// The production global previousState lives in motion_sensor.cpp.
// Expose it here so SetUp() can reset it to LOW between tests.
// ---------------------------------------------------------------------------
extern int previousState;
extern unsigned long lastNotification;
extern HardwareSerial RadarSerial;

// ---------------------------------------------------------------------------
// Fixture — resets g_digital_read_value and the production global before
// every test.
// ---------------------------------------------------------------------------

class MotionSensorTest : public ::testing::Test {
protected:
  void SetUp() override {
    g_digital_read_value = 0;
    previousState = LOW;
    lastNotification = 0;
    mock_set_millis(0);
    RadarMockState::reset();
    GpioMockState::reset();
  }
};

// ===========================================================================
// motion_get_prev_value
// ===========================================================================

TEST_F(MotionSensorTest, GetPrevValueReturnsLowInitially) {
  EXPECT_EQ(motion_get_prev_value(), LOW);
}

TEST_F(MotionSensorTest, GetPrevValueReflectsValueAfterGetValue) {
  g_digital_read_value = 1;
  motion_get_value();
  EXPECT_EQ(motion_get_prev_value(), 1);
}

// ===========================================================================
// motion_get_value
// ===========================================================================

TEST_F(MotionSensorTest, GetValueReturnsDigitalReadResult) {
  g_digital_read_value = 0;
  EXPECT_EQ(motion_get_value(), 0);

  g_digital_read_value = 1;
  EXPECT_EQ(motion_get_value(), 1);
}

TEST_F(MotionSensorTest, GetValueUpdatesPrevValueWhenChanged) {
  // previousState = LOW; first read returns 0 → different → update
  g_digital_read_value = 0;
  motion_get_value();
  EXPECT_EQ(motion_get_prev_value(), 0);

  // read returns 1 → different from 0 → update
  g_digital_read_value = 1;
  motion_get_value();
  EXPECT_EQ(motion_get_prev_value(), 1);
}

TEST_F(MotionSensorTest, GetValueDoesNotChangePrevValueWhenSame) {
  g_digital_read_value = 0;
  motion_get_value();  // sets previousState = 0
  EXPECT_EQ(motion_get_prev_value(), 0);

  // Same reading again — prev_value must stay 0.
  motion_get_value();
  EXPECT_EQ(motion_get_prev_value(), 0);
}

// ===========================================================================
// Prev vs current independence
// ===========================================================================

TEST_F(MotionSensorTest, PrevAndCurrentReadingsAreIndependent) {
  // Before any read, prev is LOW.
  EXPECT_EQ(motion_get_prev_value(), LOW);

  g_digital_read_value = 1;
  int val = motion_get_value();
  EXPECT_EQ(val, 1);
  EXPECT_EQ(motion_get_prev_value(), 1);

  // Change reading back to 0.
  g_digital_read_value = 0;
  val = motion_get_value();
  EXPECT_EQ(val, 0);
  EXPECT_EQ(motion_get_prev_value(), 0);
}

TEST_F(MotionSensorTest, InitConfiguresRadarAndMotionInput) {
  motion_init_sensor();
  const auto& radar = RadarMockState::instance();
  EXPECT_EQ(RadarSerial.baud, 256000UL);
  EXPECT_EQ(RadarSerial.config, SERIAL_8N1);
  EXPECT_EQ(radar.serial, &RadarSerial);
  EXPECT_EQ(radar.configuration_requests, 1);
  EXPECT_EQ(radar.max_moving, 8);
  EXPECT_EQ(radar.max_stationary, 0);
  EXPECT_EQ(radar.inactivity_timer, 2);
  ASSERT_EQ(radar.thresholds.size(), 9u);
  for (size_t gate = 0; gate < 9; ++gate) {
    EXPECT_EQ(radar.thresholds[gate].gate, gate);
    EXPECT_EQ(radar.thresholds[gate].moving, 55 - 5 * gate);
    EXPECT_EQ(radar.thresholds[gate].stationary, 100);
  }
  ASSERT_EQ(GpioMockState::instance().pin_modes.count(MOTION_PIN), 1u);
  EXPECT_EQ(GpioMockState::instance().pin_modes.at(MOTION_PIN), INPUT);
}

TEST_F(MotionSensorTest, FailedConfigurationReadStillAppliesRequestedConfiguration) {
  RadarMockState::instance().configuration_success = false;
  motion_init_sensor();
  EXPECT_EQ(RadarMockState::instance().max_values_calls, 1);
  EXPECT_EQ(RadarMockState::instance().thresholds.size(), 9u);
}

TEST_F(MotionSensorTest, FailedMaxValuesStopsConfiguration) {
  RadarMockState::instance().max_values_success = false;
  motion_init_sensor();
  EXPECT_TRUE(RadarMockState::instance().thresholds.empty());
  EXPECT_TRUE(GpioMockState::instance().pin_modes.empty());
}
