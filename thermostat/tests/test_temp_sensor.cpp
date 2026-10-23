#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Mock headers first so production includes resolve to stubs.
#include <Arduino.h>
#include "Wire.h"
#include "Adafruit_I2CDevice.h"
#include "Adafruit_I2CRegister.h"
#include "Adafruit_MCP9600.h"

#include "temp_sensor_config.h"
#include "secrets.h"
#include "temp_sensor.h"

#ifndef THERMOCOUPLE_TYPE
#define THERMOCOUPLE_TYPE "K"
#endif

static MCP9600_ThermocoupleType expectedThermocoupleType() {
  if (strcmp(THERMOCOUPLE_TYPE, "K") == 0) return MCP9600_TYPE_K;
  if (strcmp(THERMOCOUPLE_TYPE, "J") == 0) return MCP9600_TYPE_J;
  if (strcmp(THERMOCOUPLE_TYPE, "T") == 0) return MCP9600_TYPE_T;
  if (strcmp(THERMOCOUPLE_TYPE, "N") == 0) return MCP9600_TYPE_N;
  if (strcmp(THERMOCOUPLE_TYPE, "S") == 0) return MCP9600_TYPE_S;
  if (strcmp(THERMOCOUPLE_TYPE, "E") == 0) return MCP9600_TYPE_E;
  if (strcmp(THERMOCOUPLE_TYPE, "B") == 0) return MCP9600_TYPE_B;
  if (strcmp(THERMOCOUPLE_TYPE, "R") == 0) return MCP9600_TYPE_R;
  return MCP9600_TYPE_K;
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class TempSensorTest : public ::testing::Test {
protected:
  void SetUp() override {
    mock_set_millis(0);
    EspMockState::reset();
    TempSensorMockState::reset();
    temp_init_sensor();
    TempSensorMockState::reset();
  }
};

// ===========================================================================
// temp_init_sensor
// ===========================================================================

TEST_F(TempSensorTest, InitSensorCallsBegin) {
  temp_init_sensor();
  EXPECT_TRUE(TempSensorMockState::instance().init_called);
}

TEST_F(TempSensorTest, InitSensorSucceedsWithDefaultMockState) {
  // begin_result defaults to true — init should not trigger ESP.restart().
  TempSensorMockState::instance().begin_result = true;
  temp_init_sensor();  // must not crash or hang
  EXPECT_TRUE(TempSensorMockState::instance().init_called);
}

TEST_F(TempSensorTest, InitSensorUsesMcp9600IdentityAtConfiguredAddress) {
  temp_init_sensor();
  const auto& state = TempSensorMockState::instance();
  EXPECT_EQ(state.expected_device_id, 0x40);
  EXPECT_EQ(state.begin_address, 0x67);
  EXPECT_EQ(EspMockState::instance().restart_count, 0);
}

TEST_F(TempSensorTest, InitSensorRejectsMcp9601Identity) {
  TempSensorMockState::instance().device_id = 0x41;
  temp_init_sensor();
  EXPECT_EQ(EspMockState::instance().restart_count, 1);
}

TEST_F(TempSensorTest, InitSensorUsesConfiguredThermocoupleType) {
  temp_init_sensor();

  EXPECT_EQ(TempSensorMockState::instance().set_thermocouple_type_count, 1);
  EXPECT_EQ(TempSensorMockState::instance().thermocouple_type,
            expectedThermocoupleType());
}

// ===========================================================================
// temp_get_temperature
// ===========================================================================

TEST_F(TempSensorTest, GetTemperatureReturnsThermocoupleValue) {
  TempSensorMockState::instance().thermocouple_value = 23.5f;
  EXPECT_FLOAT_EQ(temp_get_temperature().value, 23.5f);
}

TEST_F(TempSensorTest, GetTemperatureReturnsNegativeValue) {
  TempSensorMockState::instance().thermocouple_value = -5.0f;
  EXPECT_FLOAT_EQ(temp_get_temperature().value, -5.0f);
}

TEST_F(TempSensorTest, GetTemperatureReturnsZero) {
  TempSensorMockState::instance().thermocouple_value = 0.0f;
  EXPECT_FLOAT_EQ(temp_get_temperature().value, 0.0f);
}

TEST_F(TempSensorTest, GetTemperatureReturnsNanWhenMockReturnsNan) {
  TempSensorMockState::instance().thermocouple_value = NAN;
  const TemperatureReading result = temp_get_temperature();
  EXPECT_TRUE(std::isnan(result.value));
}

TEST_F(TempSensorTest, GetTemperatureIncrementsReadCount) {
  temp_get_temperature();
  temp_get_temperature();
  EXPECT_EQ(TempSensorMockState::instance().read_count, 2);
}

TEST_F(TempSensorTest, GetTemperatureReturnsMockValueConsistently) {
  TempSensorMockState::instance().thermocouple_value = 18.75f;
  EXPECT_FLOAT_EQ(temp_get_temperature().value, 18.75f);
  EXPECT_FLOAT_EQ(temp_get_temperature().value, 18.75f);  // same value on repeated reads
}

TEST_F(TempSensorTest, ExpiresWithoutAnyAcceptedMeasurement) {
  TempSensorMockState::instance().thermocouple_value = NAN;
  const TemperatureReading reading = temp_get_temperature();
  EXPECT_TRUE(std::isnan(reading.value));
  EXPECT_TRUE(reading.expired);
}

TEST_F(TempSensorTest, CachedRejectedValuesDoNotRefreshAcceptedAge) {
  const unsigned long timeout = TEMP_MAX_ACCEPTED_AGE_SECONDS * 1000UL;
  TempSensorMockState::instance().thermocouple_value = 25.0f;
  TemperatureReading reading = temp_get_temperature();
  EXPECT_FLOAT_EQ(reading.value, 25.0f);
  EXPECT_FALSE(reading.expired);

  mock_advance_millis(timeout - 1);
  TempSensorMockState::instance().thermocouple_value = NAN;
  reading = temp_get_temperature();
  EXPECT_FLOAT_EQ(reading.value, 25.0f);
  EXPECT_FALSE(reading.expired);

  mock_advance_millis(1);
  TempSensorMockState::instance().thermocouple_value = NAN;
  reading = temp_get_temperature();
  EXPECT_FLOAT_EQ(reading.value, 25.0f);
  EXPECT_TRUE(reading.expired);
}

TEST_F(TempSensorTest, UnchangedAcceptedReadingsKeepRefreshingAge) {
  const unsigned long timeout = TEMP_MAX_ACCEPTED_AGE_SECONDS * 1000UL;
  TempSensorMockState::instance().thermocouple_value = 25.0f;
  for (int i = 0; i < 10; ++i) {
    const TemperatureReading reading = temp_get_temperature();
    EXPECT_FLOAT_EQ(reading.value, 25.0f);
    EXPECT_FALSE(reading.expired);
    mock_advance_millis(timeout - 1);
  }
  TempSensorMockState::instance().thermocouple_value = NAN;
  EXPECT_FALSE(temp_get_temperature().expired);
  mock_advance_millis(1);
  EXPECT_TRUE(temp_get_temperature().expired);
}

TEST_F(TempSensorTest, RejectsInfinityWithoutRefreshingAge) {
  TempSensorMockState::instance().thermocouple_value = INFINITY;
  TemperatureReading reading = temp_get_temperature();
  EXPECT_TRUE(std::isnan(reading.value));
  EXPECT_TRUE(reading.expired);
  TempSensorMockState::instance().thermocouple_value = 25.0f;
  temp_get_temperature();
  mock_advance_millis(TEMP_MAX_ACCEPTED_AGE_SECONDS * 1000UL);
  TempSensorMockState::instance().thermocouple_value = -INFINITY;
  reading = temp_get_temperature();
  EXPECT_FLOAT_EQ(reading.value, 25.0f);
  EXPECT_TRUE(reading.expired);
}

TEST_F(TempSensorTest, AcceptedMeasurementCanRecoverAgeButInitClearsIt) {
  TempSensorMockState::instance().thermocouple_value = 25.0f;
  temp_get_temperature();
  mock_advance_millis(TEMP_MAX_ACCEPTED_AGE_SECONDS * 1000UL);
  TempSensorMockState::instance().thermocouple_value = NAN;
  EXPECT_TRUE(temp_get_temperature().expired);
  TempSensorMockState::instance().thermocouple_value = 25.0f;
  EXPECT_FALSE(temp_get_temperature().expired);
  temp_init_sensor();
  TempSensorMockState::instance().thermocouple_value = NAN;
  EXPECT_TRUE(temp_get_temperature().expired);
}

TEST_F(TempSensorTest, AgeHandlesEsp32MillisRollover) {
  const unsigned long timeout = TEMP_MAX_ACCEPTED_AGE_SECONDS * 1000UL;
  mock_set_millis(UINT32_MAX - timeout / 2);
  TempSensorMockState::instance().thermocouple_value = 25.0f;
  temp_get_temperature();
  TempSensorMockState::instance().thermocouple_value = NAN;
  mock_advance_millis(timeout - 1);
  EXPECT_FALSE(temp_get_temperature().expired);
  mock_advance_millis(1);
  EXPECT_TRUE(temp_get_temperature().expired);
}

TEST_F(TempSensorTest, AppliesConfiguredDeviationToHistoryAverage) {
  TempSensorMockState::instance().thermocouple_value = 100.0f;
  temp_get_temperature();
  // With filtering disabled, a large change is accepted and refreshes its age.
  mock_advance_millis(TEMP_MAX_ACCEPTED_AGE_SECONDS * 1000UL);
  TempSensorMockState::instance().thermocouple_value = 125.0f;
  const TemperatureReading reading = temp_get_temperature();
  if (TEMP_VALID_MAX_DEVIATION_PERCENT == 0 || TEMP_VALID_MAX_DEVIATION_PERCENT >= 25) {
    EXPECT_FLOAT_EQ(reading.value, 125.0f);
    EXPECT_FALSE(reading.expired);
  } else {
    EXPECT_FLOAT_EQ(reading.value, 100.0f);
    EXPECT_TRUE(reading.expired);
  }
}

TEST_F(TempSensorTest, AcceptsDeviationAtConfiguredBoundary) {
  TempSensorMockState::instance().thermocouple_value = 100.0f;
  temp_get_temperature();
  const float boundary = 100.0f + TEMP_VALID_MAX_DEVIATION_PERCENT;
  TempSensorMockState::instance().thermocouple_value = boundary;
  EXPECT_FLOAT_EQ(temp_get_temperature().value, boundary);
}
