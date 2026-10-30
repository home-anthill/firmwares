#include "config.h"
#include "secrets.h"
#include "barometer_sensor.h"

#include <Arduino.h>
#include <Wire.h>
#include <bsec2.h>

constexpr float SEA_LEVEL_PRESSURE_HPA = 1013.25f;
constexpr unsigned long MAX_SAMPLE_AGE_MS = 10000;

static Bsec2 bme;
static bool ready = false;
static bool has_sample = false;
static unsigned long last_sample_ms = 0;
static BarometerReading latest;

static void new_data(const bme68xData data, const bsecOutputs outputs, Bsec2 sensor) {
  if (outputs.nOutputs == 0) {
    return;
  }
  BarometerReading reading;
  // bsec2 1.10 normalizes the callback pressure to hPa before invoking us.
  if (isfinite(data.pressure) && data.pressure > 0) {
    reading.airpressure = data.pressure;
    reading.altitude = 44330.0f * (1.0f - powf(reading.airpressure / SEA_LEVEL_PRESSURE_HPA, 0.1903f));
  }
  for (uint8_t i = 0; i < outputs.nOutputs; ++i) {
    const bsecData& output = outputs.output[i];
    if (!isfinite(output.signal)) {
      continue;
    }
    switch (output.sensor_id) {
      // Accuracy 0 means BSEC is still stabilizing; do not publish these estimates yet.
      case BSEC_OUTPUT_IAQ:
        if (output.accuracy > 0 && output.signal >= 0 && output.signal <= 500) {
          reading.iaq = output.signal;
        }
        break;
      case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
        if (output.accuracy > 0 && output.signal >= 0) {
          reading.voc = output.signal;
        }
        break;
      case BSEC_OUTPUT_CO2_EQUIVALENT:
        if (output.accuracy > 0 && output.signal > 0) {
          reading.eco2 = output.signal;
        }
        break;
      default:
        break;
    }
  }
  latest = reading;
  last_sample_ms = millis();
  has_sample = true;
}

void barometer_init_sensor() {
  ready = false;
  has_sample = false;
  latest = {};
  if (!Wire.begin(I2C_SDA, I2C_SCL, 100000)) {
    Serial.println("barometer_init_sensor - I2C initialization failed");
    return;
  }
  if (!bme.begin(BME68X_I2C_ADDR_LOW, Wire) && !bme.begin(BME68X_I2C_ADDR_HIGH, Wire)) {
    Serial.println("barometer_init_sensor - BME688 not found at 0x76 or 0x77");
    return;
  }
  bsecSensor outputs[] = {
    BSEC_OUTPUT_RAW_PRESSURE, BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT, BSEC_OUTPUT_CO2_EQUIVALENT
  };
  ready = bme.updateSubscription(outputs, sizeof(outputs) / sizeof(outputs[0]), BSEC_SAMPLE_RATE_LP);
  if (!ready) {
    Serial.printf("barometer_init_sensor - BSEC subscription failed: %d\n", bme.status);
    return;
  }
  bme.attachCallback(new_data);
}

void barometer_update() {
  if (ready && !bme.run()) {
    latest = {};
    has_sample = false;
    Serial.printf("barometer_update - BSEC status=%d sensor status=%d\n", bme.status, bme.sensor.status);
  }
}

BarometerReading barometer_read() {
  if (!ready || !has_sample || millis() - last_sample_ms > MAX_SAMPLE_AGE_MS) {
    return {};
  }
  return latest;
}
