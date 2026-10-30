#include <gtest/gtest.h>
#include <cmath>
#include "SensirionI2cScd4x.h"
#include "co2_sensor.h"
#include "config.h"
#include "secrets.h"

class Co2SensorTest : public ::testing::Test {
protected:
  void SetUp() override { Scd43MockState::reset(); Wire = TwoWire{}; }
};

TEST_F(Co2SensorTest, InitializesSharedBusAndRestartsPeriodicMeasurementInOrder) {
  co2_init_sensor();
  const auto& state = Scd43MockState::instance();
  EXPECT_EQ(Wire.sda, I2C_SDA);
  EXPECT_EQ(Wire.scl, I2C_SCL);
  EXPECT_EQ(Wire.clock, 100000u);
  EXPECT_EQ(state.bus, &Wire);
  EXPECT_EQ(state.address, 0x62);
  EXPECT_EQ(state.operations, (std::vector<std::string>{"begin", "wake", "stop", "serial", "start"}));
}

TEST_F(Co2SensorTest, ReturnsNewCo2SampleOnlyOnce) {
  co2_init_sensor();
  EXPECT_FLOAT_EQ(co2_read(), 800.0f);
  EXPECT_TRUE(std::isnan(co2_read()));
  EXPECT_EQ(Scd43MockState::instance().read_count, 1);
}

TEST_F(Co2SensorTest, DataNotReadyDoesNotReadMeasurement) {
  co2_init_sensor();
  Scd43MockState::instance().data_ready = false;
  EXPECT_TRUE(std::isnan(co2_read()));
  EXPECT_EQ(Scd43MockState::instance().read_count, 0);
}

TEST_F(Co2SensorTest, StatusErrorDoesNotReturnOrReadAnOldMeasurement) {
  co2_init_sensor();
  EXPECT_FLOAT_EQ(co2_read(), 800.0f);
  Scd43MockState::instance().data_ready = true;
  Scd43MockState::instance().status_error = 1;
  EXPECT_TRUE(std::isnan(co2_read()));
  EXPECT_EQ(Scd43MockState::instance().read_count, 1);
}

TEST_F(Co2SensorTest, ReadErrorIsInvalidAndSubsequentSampleCanRecover) {
  co2_init_sensor();
  Scd43MockState::instance().read_error = 1;
  EXPECT_TRUE(std::isnan(co2_read()));
  Scd43MockState::instance().read_error = 0;
  Scd43MockState::instance().data_ready = true;
  EXPECT_FLOAT_EQ(co2_read(), 800.0f);
}

TEST_F(Co2SensorTest, ZeroAndValuesBeyondOutputRangeAreRejected) {
  co2_init_sensor();
  for (uint16_t value : {uint16_t(0), uint16_t(40001), uint16_t(65535)}) {
    Scd43MockState::instance().co2 = value;
    Scd43MockState::instance().data_ready = true;
    EXPECT_TRUE(std::isnan(co2_read()));
  }
  Scd43MockState::instance().co2 = 40000;
  Scd43MockState::instance().data_ready = true;
  EXPECT_FLOAT_EQ(co2_read(), 40000.0f);
}

TEST_F(Co2SensorTest, StartupFailureDoesNotAttemptReadsAndReinitCanRecover) {
  Scd43MockState::instance().start_error = 1;
  co2_init_sensor();
  EXPECT_TRUE(std::isnan(co2_read()));
  EXPECT_EQ(Scd43MockState::instance().status_count, 0);
  Scd43MockState::instance().start_error = 0;
  co2_init_sensor();
  EXPECT_FLOAT_EQ(co2_read(), 800.0f);
}

TEST_F(Co2SensorTest, WakeStopOrSerialErrorsDoNotHaltOtherStartupSteps) {
  Scd43MockState::instance().wake_error = 1;
  Scd43MockState::instance().stop_error = 1;
  Scd43MockState::instance().serial_error = 1;
  co2_init_sensor();
  EXPECT_EQ(Scd43MockState::instance().operations.back(), "start");
  EXPECT_FLOAT_EQ(co2_read(), 800.0f);
}

TEST_F(Co2SensorTest, BusFailureSkipsSensorAndReinitializationCanRecover) {
  Wire.begin_success = false;
  co2_init_sensor();
  EXPECT_TRUE(Scd43MockState::instance().operations.empty());
  EXPECT_TRUE(std::isnan(co2_read()));
  EXPECT_EQ(Scd43MockState::instance().status_count, 0);
  Wire.begin_success = true;
  co2_init_sensor();
  EXPECT_FLOAT_EQ(co2_read(), 800.0f);
}
