#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include "Arduino.h"
#include "Preferences.h"
#include "Adafruit_ADS1X15.h"
#include "ph_sensor.h"
#include "config.h"
#include "secrets.h"

static std::string screen[4];
static std::vector<std::string> titles;
void display_set_overlay(const char* a, const char* b, const char* c, const char* d) {
  titles.push_back(a);
  screen[0] = a; screen[1] = b; screen[2] = c; screen[3] = d;
}
void display_clear_overlay() {}

// Wire-compatible dev-kit record, kept as a single NVS blob.
struct SavedCalibration { uint32_t version; float ph1, ph2, v1, v2; };

class PhSensorTest : public ::testing::Test {
protected:
  void SetUp() override {
    Preferences::reset();
    AdcMockState::reset();
    Wire = TwoWire{};
    GpioMockState::reset();
    g_digital_read_value = HIGH;
    mock_set_millis(0);
    titles.clear();
    for (auto& line : screen) { line.clear(); }
  }
  void store(SavedCalibration calibration = {1, 7, 4.01f, 0, 0.177f}) {
    Preferences memory;
    memory.begin("ph-meter", false);
    memory.putBytes("cal", &calibration, sizeof(calibration));
    pref_store::byte_writes = 0;
  }
  void tick(uint32_t duration) {
    for (uint32_t elapsed = 0; elapsed < duration; elapsed += 10) {
      mock_advance_millis(10);
      ph_sensor_update();
    }
  }
  void tap() {
    g_digital_read_value = LOW; tick(100);
    g_digital_read_value = HIGH; tick(100);
  }
  void hold() {
    g_digital_read_value = LOW; tick(3100);
    g_digital_read_value = HIGH; tick(100);
  }
  void stable(int16_t raw) {
    AdcMockState::instance().raw = raw;
    tick(5500);
    ASSERT_TRUE(ph_read().stable);
  }
  void capture_pair(bool alkaline = false) {
    hold();
    ASSERT_EQ(screen[0], "Choose buffer pair");
    if (alkaline) {
      tap();
    }
    hold();
    ASSERT_EQ(screen[0], "Immerse in pH 7.00");
    stable(0);
    tap();
    ASSERT_EQ(screen[0], alkaline ? "Immerse in pH 10.01" : "Immerse in pH 4.01");
    stable(alkaline ? -5683 : 5664);
    tap();
    ASSERT_EQ(screen[0], "Save calibration?");
  }
};

TEST_F(PhSensorTest, InitUsesConfiguredPinsDifferentialGainAndRate) {
  ph_init_sensor();
  EXPECT_EQ(Wire.sda, I2C_SDA);
  EXPECT_EQ(Wire.scl, I2C_SCL);
  EXPECT_EQ(Wire.clock, 100000u);
  EXPECT_EQ(GpioMockState::instance().pin_modes.at(PH_CALIBRATION_BUTTON_PIN), INPUT_PULLUP);
  EXPECT_EQ(AdcMockState::instance().address, 0x48);
  EXPECT_EQ(AdcMockState::instance().gain, GAIN_FOUR);
  EXPECT_EQ(AdcMockState::instance().rate, RATE_ADS1115_16SPS);
  tick(300);
  EXPECT_EQ(AdcMockState::instance().mux, ADS1X15_REG_CONFIG_MUX_DIFF_0_1);
  EXPECT_FALSE(AdcMockState::instance().continuous);
}

TEST_F(PhSensorTest, UncalibratedSensorHasVoltageButNeverPh) {
  ph_init_sensor();
  stable(100);
  EXPECT_TRUE(std::isfinite(ph_read().voltage));
  EXPECT_TRUE(std::isnan(ph_read().ph));
  EXPECT_FALSE(ph_read().calibrated);
  EXPECT_EQ(pref_store::byte_writes, 0);
}

TEST_F(PhSensorTest, LoadsDevKitRecordAndCalculatesInterpolatedPh) {
  store(); ph_init_sensor(); stable(2832);
  EXPECT_TRUE(ph_read().calibrated);
  EXPECT_NEAR(ph_read().ph, 5.505f, 0.001f);
}

TEST_F(PhSensorTest, InvalidStoredVersionReversedOrImplausibleSlopeIsRejected) {
  for (SavedCalibration record : {
      SavedCalibration{2, 7, 4.01f, 0, 0.177f},
      SavedCalibration{1, 7, 4.01f, 0, -0.177f},
      SavedCalibration{1, 7, 4.01f, 0, 0.001f},
      SavedCalibration{1, 7, 4.01f, 0, 0.5f},
      SavedCalibration{1, 7, 4.01f, NAN, 0.177f},
      SavedCalibration{1, 7, 5, 0, 0.177f}}) {
    store(record); ph_init_sensor();
    EXPECT_FALSE(ph_read().calibrated);
  }
}

TEST_F(PhSensorTest, AcidPairOnlySavesAfterConfirmationAndSurvivesReinit) {
  ph_init_sensor(); capture_pair();
  EXPECT_EQ(pref_store::byte_writes, 0);
  EXPECT_TRUE(ph_read().calibrating);
  EXPECT_TRUE(std::isnan(ph_read().ph));
  tap();
  EXPECT_EQ(pref_store::byte_writes, 1);
  EXPECT_TRUE(ph_read().calibrated);
  EXPECT_FALSE(ph_read().calibrating);
  ph_init_sensor(); stable(0);
  EXPECT_NEAR(ph_read().ph, 7, 0.001f);
}

TEST_F(PhSensorTest, AlkalinePairCanBeSaved) {
  ph_init_sensor(); capture_pair(true); tap();
  EXPECT_TRUE(ph_read().calibrated);
  EXPECT_NEAR(ph_read().ph, 10.01f, 0.01f);
}

TEST_F(PhSensorTest, CancelRetainsPreviousRecordWithoutFlashWrite) {
  store(); ph_init_sensor(); hold(); hold();
  hold();
  EXPECT_FALSE(ph_read().calibrating);
  EXPECT_TRUE(ph_read().calibrated);
  EXPECT_EQ(pref_store::byte_writes, 0);
  stable(0);
  EXPECT_NEAR(ph_read().ph, 7, 0.001f);
}

TEST_F(PhSensorTest, UnstablePointAndInvalidSlopeCannotBeSaved) {
  ph_init_sensor(); hold(); hold(); tap();
  EXPECT_EQ(screen[0], "Immerse in pH 7.00");
  stable(0); tap(); stable(-5664); tap();
  EXPECT_NE(std::find(titles.begin(), titles.end(), "Invalid calibration"), titles.end());
  EXPECT_TRUE(ph_read().calibrating);
  EXPECT_EQ(pref_store::byte_writes, 0);
  EXPECT_FALSE(ph_read().calibrated);
}

TEST_F(PhSensorTest, WriteFailureAllowsRetryWithoutReplacingActiveCalibration) {
  store(); ph_init_sensor(); capture_pair();
  pref_store::write_success = false; tap();
  EXPECT_TRUE(ph_read().calibrating);
  EXPECT_TRUE(ph_read().calibrated);
  pref_store::write_success = true; tap();
  EXPECT_FALSE(ph_read().calibrating);
  EXPECT_TRUE(ph_read().calibrated);
}

TEST_F(PhSensorTest, MissingAdcOrFlashDoesNotHaltOrInventCalibration) {
  AdcMockState::instance().begin_success = false;
  pref_store::begin_success = false;
  ph_init_sensor(); tick(1000);
  EXPECT_TRUE(std::isnan(ph_read().ph));
  EXPECT_EQ(AdcMockState::instance().starts, 0);
}

TEST_F(PhSensorTest, SaturationAndConversionTimeoutInvalidateAndResetStability) {
  store(); ph_init_sensor(); stable(0);
  AdcMockState::instance().raw = 32767; tick(300);
  EXPECT_TRUE(std::isnan(ph_read().ph));
  EXPECT_FALSE(ph_read().stable);
  stable(0);
  AdcMockState::instance().complete = false; tick(500);
  EXPECT_TRUE(std::isnan(ph_read().ph));
  EXPECT_FALSE(ph_read().stable);
}

TEST_F(PhSensorTest, StaleSampleAndCalibrationModeDoNotReturnPublishablePh) {
  store(); ph_init_sensor(); stable(0);
  mock_advance_millis(2000);
  EXPECT_TRUE(std::isnan(ph_read().ph));
  tick(1000); hold();
  EXPECT_TRUE(ph_read().calibrating);
  EXPECT_TRUE(std::isnan(ph_read().ph));
}

TEST_F(PhSensorTest, ButtonBounceDoesNotEnterCalibration) {
  ph_init_sensor();
  for (int i = 0; i < 10; ++i) {
    g_digital_read_value = LOW; tick(20);
    g_digital_read_value = HIGH; tick(20);
  }
  EXPECT_FALSE(ph_read().calibrating);
}

TEST_F(PhSensorTest, FluctuatingVoltageNeverBecomesStable) {
  ph_init_sensor();
  for (int i = 0; i < 30; ++i) {
    AdcMockState::instance().raw = i % 2 ? 100 : 0;
    tick(250);
  }
  EXPECT_FALSE(ph_read().stable);
}

TEST_F(PhSensorTest, NegativeSaturationAlsoInvalidatesMeasurement) {
  store(); ph_init_sensor(); stable(0);
  AdcMockState::instance().raw = -32768; tick(300);
  EXPECT_TRUE(std::isnan(ph_read().ph));
}
