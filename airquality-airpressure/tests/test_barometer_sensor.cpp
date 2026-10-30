#include <gtest/gtest.h>
#include <cmath>
#include "Arduino.h"
#include "Wire.h"
#include "bsec2.h"
#include "config.h"
#include "secrets.h"
#include "barometer_sensor.h"

class BarometerSensorTest : public ::testing::Test {
protected:
  void SetUp() override {
    BsecMockState::reset();
    Wire = TwoWire{};
    mock_set_millis(0);
  }
};

TEST_F(BarometerSensorTest, InitUsesConfiguredBusAndBsecLowPowerOutputs) {
  barometer_init_sensor();
  EXPECT_EQ(Wire.sda, I2C_SDA);
  EXPECT_EQ(Wire.scl, I2C_SCL);
  EXPECT_EQ(Wire.clock, 100000u);
  EXPECT_EQ(BsecMockState::instance().subscriptions.size(), 4u);
  EXPECT_FLOAT_EQ(BsecMockState::instance().sample_rate, BSEC_SAMPLE_RATE_LP);
}

TEST_F(BarometerSensorTest, FindsSensorAtAlternateAddress) {
  BsecMockState::instance().low_available = false;
  BsecMockState::instance().high_available = true;
  barometer_init_sensor();
  barometer_update();
  EXPECT_EQ(BsecMockState::instance().addresses, (std::vector<uint8_t>{0x76, 0x77}));
  EXPECT_FLOAT_EQ(barometer_read().airpressure, 1013.25f);
}

TEST_F(BarometerSensorTest, PublishesPressureAltitudeAndCalibratedAirQuality) {
  barometer_init_sensor();
  EXPECT_TRUE(std::isnan(barometer_read().airpressure));
  barometer_update();
  const auto reading = barometer_read();
  EXPECT_FLOAT_EQ(reading.airpressure, 1013.25f);
  EXPECT_NEAR(reading.altitude, 0.0f, 0.01f);
  EXPECT_FLOAT_EQ(reading.iaq, 75.5f);
  EXPECT_FLOAT_EQ(reading.voc, 0.75f);
  EXPECT_FLOAT_EQ(reading.eco2, 650.0f);
}

TEST_F(BarometerSensorTest, DerivesAltitudeAboveAndBelowSeaLevel) {
  barometer_init_sensor();
  BsecMockState::instance().pressure = 900;
  barometer_update();
  EXPECT_NEAR(barometer_read().altitude, 988.65f, 0.1f);
  BsecMockState::instance().pressure = 1100;
  barometer_update();
  EXPECT_LT(barometer_read().altitude, 0.0f);
}

TEST_F(BarometerSensorTest, UncalibratedGasEstimatesAreUnavailableWhilePressureRemainsValid) {
  barometer_init_sensor();
  BsecMockState::instance().accuracy = 0;
  barometer_update();
  EXPECT_FLOAT_EQ(barometer_read().airpressure, 1013.25f);
  EXPECT_TRUE(std::isnan(barometer_read().iaq));
  EXPECT_TRUE(std::isnan(barometer_read().voc));
  EXPECT_TRUE(std::isnan(barometer_read().eco2));
}

TEST_F(BarometerSensorTest, FailedAndStaleSamplesCannotReuseOldReadings) {
  barometer_init_sensor();
  barometer_update();
  BsecMockState::instance().fresh_data = false;
  mock_advance_millis(10001);
  EXPECT_TRUE(std::isnan(barometer_read().airpressure));
  BsecMockState::instance().fresh_data = true;
  barometer_update();
  EXPECT_FLOAT_EQ(barometer_read().iaq, 75.5f);
  BsecMockState::instance().run_success = false;
  barometer_update();
  EXPECT_TRUE(std::isnan(barometer_read().iaq));
}

TEST_F(BarometerSensorTest, FailedInitializationOrSubscriptionLeavesValuesUnavailable) {
  BsecMockState::instance().low_available = false;
  barometer_init_sensor();
  barometer_update();
  EXPECT_TRUE(std::isnan(barometer_read().iaq));
  BsecMockState::instance().low_available = true;
  BsecMockState::instance().subscription_success = false;
  barometer_init_sensor();
  barometer_update();
  EXPECT_TRUE(std::isnan(barometer_read().iaq));
  Wire.begin_success = false;
  barometer_init_sensor();
  EXPECT_TRUE(std::isnan(barometer_read().airpressure));
}

TEST_F(BarometerSensorTest, InvalidFieldsAreNotPublished) {
  barometer_init_sensor();
  BsecMockState::instance().pressure = 0;
  BsecMockState::instance().iaq = 501;
  BsecMockState::instance().voc = NAN;
  BsecMockState::instance().eco2 = -1;
  barometer_update();
  const auto reading = barometer_read();
  EXPECT_TRUE(std::isnan(reading.airpressure));
  EXPECT_TRUE(std::isnan(reading.altitude));
  EXPECT_TRUE(std::isnan(reading.iaq));
  EXPECT_TRUE(std::isnan(reading.voc));
  EXPECT_TRUE(std::isnan(reading.eco2));
}
