#include <gtest/gtest.h>

// Mock headers first so production includes resolve to stubs.
#include <Arduino.h>

#include "pir_sensor.h"

// ---------------------------------------------------------------------------
// The production global current_pir_value lives in pir_sensor.cpp.
// Expose it here so SetUp() can reset it to -1 between tests.
// ---------------------------------------------------------------------------
extern int current_pir_value;

// ---------------------------------------------------------------------------
// Fixture — resets g_digital_read_value and the production global before
// every test.
// ---------------------------------------------------------------------------

class PirSensorTest : public ::testing::Test {
protected:
  void SetUp() override {
    g_digital_read_value = 0;
    current_pir_value    = -1;
  }
};

// ===========================================================================
// pir_get_prev_value
// ===========================================================================

TEST_F(PirSensorTest, GetPrevValueReturnsMinusOneInitially) {
  EXPECT_EQ(pir_get_prev_value(), -1);
}

TEST_F(PirSensorTest, GetPrevValueReflectsValueAfterGetValue) {
  g_digital_read_value = 1;
  pir_get_value();
  EXPECT_EQ(pir_get_prev_value(), 1);
}

// ===========================================================================
// pir_get_value
// ===========================================================================

TEST_F(PirSensorTest, GetValueReturnsDigitalReadResult) {
  g_digital_read_value = 0;
  EXPECT_EQ(pir_get_value(), 0);

  g_digital_read_value = 1;
  EXPECT_EQ(pir_get_value(), 1);
}

TEST_F(PirSensorTest, GetValueUpdatesPrevValueWhenChanged) {
  // current_pir_value = -1; first read returns 0 → different → update
  g_digital_read_value = 0;
  pir_get_value();
  EXPECT_EQ(pir_get_prev_value(), 0);

  // read returns 1 → different from 0 → update
  g_digital_read_value = 1;
  pir_get_value();
  EXPECT_EQ(pir_get_prev_value(), 1);
}

TEST_F(PirSensorTest, GetValueDoesNotChangePrevValueWhenSame) {
  g_digital_read_value = 0;
  pir_get_value();  // sets current_pir_value = 0
  EXPECT_EQ(pir_get_prev_value(), 0);

  // Same reading again — prev_value must stay 0.
  pir_get_value();
  EXPECT_EQ(pir_get_prev_value(), 0);
}

// ===========================================================================
// Prev vs current independence
// ===========================================================================

TEST_F(PirSensorTest, PrevAndCurrentReadingsAreIndependent) {
  // Before any read, prev is -1.
  EXPECT_EQ(pir_get_prev_value(), -1);

  g_digital_read_value = 1;
  int val = pir_get_value();
  EXPECT_EQ(val, 1);
  EXPECT_EQ(pir_get_prev_value(), 1);

  // Change reading back to 0.
  g_digital_read_value = 0;
  val = pir_get_value();
  EXPECT_EQ(val, 0);
  EXPECT_EQ(pir_get_prev_value(), 0);
}
