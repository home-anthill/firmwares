// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>

#include <math.h>
#include <string.h>

// Thermocouple MCP9600
// include specific libraries:
// - OneWire: https://github.com/PaulStoffregen/OneWire
// - DallasTemperature: https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <Adafruit_I2CDevice.h>
#include <Adafruit_I2CRegister.h>
#include "Adafruit_MCP9600.h"

#include "secrets.h"

#ifndef THERMOCOUPLE_TYPE
#define THERMOCOUPLE_TYPE "K"
#endif

#define I2C_ADDRESS (0x67)
#define I2C_SDA 39
#define I2C_SCL 40
Adafruit_MCP9600 mcp;
/* Set and print ambient resolution */
Ambient_Resolution ambientRes = RES_ZERO_POINT_0625;

// Thermocouple readings can occasionally jump because the signal is tiny and
// easy to disturb with motor/relay noise. Keep a short history of accepted
// values and reject sudden outliers, returning the last valid value instead.
const size_t TEMP_VALID_HISTORY_SIZE = 10;
const float TEMP_VALID_MAX_DEVIATION_PERCENT = 8.0f;
float temp_valid_history[TEMP_VALID_HISTORY_SIZE] = {};
size_t temp_valid_history_count = 0;
size_t temp_valid_history_index = 0;
float last_temp_valid_value = NAN;

static auto configured_thermocouple_type() {
  if (strcmp(THERMOCOUPLE_TYPE, "K") == 0) return MCP9600_TYPE_K;
  if (strcmp(THERMOCOUPLE_TYPE, "J") == 0) return MCP9600_TYPE_J;
  if (strcmp(THERMOCOUPLE_TYPE, "T") == 0) return MCP9600_TYPE_T;
  if (strcmp(THERMOCOUPLE_TYPE, "N") == 0) return MCP9600_TYPE_N;
  if (strcmp(THERMOCOUPLE_TYPE, "S") == 0) return MCP9600_TYPE_S;
  if (strcmp(THERMOCOUPLE_TYPE, "E") == 0) return MCP9600_TYPE_E;
  if (strcmp(THERMOCOUPLE_TYPE, "B") == 0) return MCP9600_TYPE_B;
  if (strcmp(THERMOCOUPLE_TYPE, "R") == 0) return MCP9600_TYPE_R;

  Serial.printf("Unsupported THERMOCOUPLE_TYPE \"%s\", defaulting to K\n", THERMOCOUPLE_TYPE);
  return MCP9600_TYPE_K;
}

static float get_temp_history_avg() {
  if (temp_valid_history_count == 0) {
    return NAN;
  }

  float total = 0.0f;
  for (size_t i = 0; i < temp_valid_history_count; i++) {
    total += temp_valid_history[i];
  }
  return total / temp_valid_history_count;
}

static void set_temp_valid_value(float value) {
  temp_valid_history[temp_valid_history_index] = value;
  temp_valid_history_index = (temp_valid_history_index + 1) % TEMP_VALID_HISTORY_SIZE;
  if (temp_valid_history_count < TEMP_VALID_HISTORY_SIZE) {
    temp_valid_history_count++;
  }
  last_temp_valid_value = value;
}

static void reset_valid_temp_history() {
  for (size_t i = 0; i < TEMP_VALID_HISTORY_SIZE; i++) {
    temp_valid_history[i] = 0.0f;
  }
  temp_valid_history_count = 0;
  temp_valid_history_index = 0;
  last_temp_valid_value = NAN;
}

static bool is_temp_reading_is_accettable(float value) {
  if (isnan(value)) {
    return false;
  }

  // Bootstrap: without accepted history there is no trustworthy avg yet.
  if (temp_valid_history_count == 0 || isnan(last_temp_valid_value)) {
    return true;
  }

  float avg = get_temp_history_avg();
  float max_delta = fabs(avg) * (TEMP_VALID_MAX_DEVIATION_PERCENT / 100.0f);

  return fabs(value - avg) <= max_delta;
}

void temp_init_sensor() {
  Serial.println("temp_init_sensor - called");
  reset_valid_temp_history();
  // init display
  Wire.setPins(I2C_SDA, I2C_SCL);

  // init temperature sensor
  /* Initialise the driver with I2C_ADDRESS and the default I2C bus. */
  if (! mcp.begin(I2C_ADDRESS)) {
    Serial.println("Sensor not found. Check wiring! Restarting in 10 seconds...");
    delay(10000);
    ESP.restart();
  }

  Serial.println("Found MCP9600!");

  /* Set and print ambient resolution */
  mcp.setAmbientResolution(ambientRes);
  Serial.print("Ambient Resolution set to: ");
  switch (ambientRes) {
    case RES_ZERO_POINT_25:    Serial.println("0.25°C"); break;
    case RES_ZERO_POINT_125:   Serial.println("0.125°C"); break;
    case RES_ZERO_POINT_0625:  Serial.println("0.0625°C"); break;
    case RES_ZERO_POINT_03125: Serial.println("0.03125°C"); break;
  }

  mcp.setADCresolution(MCP9600_ADCRESOLUTION_18);
  Serial.print("ADC resolution set to ");
  switch (mcp.getADCresolution()) {
    case MCP9600_ADCRESOLUTION_18:   Serial.print("18"); break;
    case MCP9600_ADCRESOLUTION_16:   Serial.print("16"); break;
    case MCP9600_ADCRESOLUTION_14:   Serial.print("14"); break;
    case MCP9600_ADCRESOLUTION_12:   Serial.print("12"); break;
  }
  Serial.println(" bits");

  mcp.setThermocoupleType(configured_thermocouple_type());
  Serial.print("Thermocouple type set to ");
  switch (mcp.getThermocoupleType()) {
    case MCP9600_TYPE_K:  Serial.print("K"); break;
    case MCP9600_TYPE_J:  Serial.print("J"); break;
    case MCP9600_TYPE_T:  Serial.print("T"); break;
    case MCP9600_TYPE_N:  Serial.print("N"); break;
    case MCP9600_TYPE_S:  Serial.print("S"); break;
    case MCP9600_TYPE_E:  Serial.print("E"); break;
    case MCP9600_TYPE_B:  Serial.print("B"); break;
    case MCP9600_TYPE_R:  Serial.print("R"); break;
  }
  Serial.println(" type");

  mcp.setFilterCoefficient(3);
  Serial.print("Filter coefficient value set to: ");
  Serial.println(mcp.getFilterCoefficient());

  mcp.setAlertTemperature(1, 30);
  Serial.print("Alert #1 temperature set to ");
  Serial.println(mcp.getAlertTemperature(1));
  mcp.configureAlert(1, true, true);  // alert 1 enabled, rising temp

  mcp.enable(true);

  Serial.println(F("------------------------------"));
}

float temp_get_temperature() {
  float thermocouple = mcp.readThermocouple();
  float ambient = mcp.readAmbient();
  int32_t adc = mcp.readADC() * 2;
  Serial.print("Hot Junction: "); Serial.println(thermocouple);
  Serial.print("Cold Junction: "); Serial.println(ambient);
  Serial.print("ADC: "); Serial.print(adc); Serial.println(" uV");

  // Use the raw value only when it agrees with the accepted recent history.
  // If something external produces a sudden spike, keep controlling with the last
  // valid value so one or more bad reads do not flip the GPIO outputs.
  if (is_temp_reading_is_accettable(thermocouple)) {
    set_temp_valid_value(thermocouple);
    return thermocouple;
  }

  Serial.print("temp_get_temperature - rejected bad thermocouple reading: ");
  Serial.print(thermocouple);
  Serial.print(" C; accepted history average: ");
  Serial.print(get_temp_history_avg());
  Serial.print(" C; returning previous valid value: ");
  Serial.print(last_temp_valid_value);
  Serial.println(" C");

  return last_temp_valid_value;
}
