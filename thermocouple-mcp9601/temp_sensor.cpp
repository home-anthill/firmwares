#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "config.h"
#include "temp_sensor.h"

#include <Adafruit_MCP9601.h>
static Adafruit_MCP9601 mcp;
static bool sensor_ready = false;

void temp_init_sensor() {
  sensor_ready = false;
  Wire.setPins(I2C_SDA, I2C_SCL);
  if (!mcp.begin(THERMOCOUPLE_I2C_ADDRESS)) {
    Serial.println("MCP9601 not found or incorrect device identity");
    return;
  }
  mcp.setAmbientResolution(RES_ZERO_POINT_0625);
  mcp.setADCresolution(MCP9600_ADCRESOLUTION_18);
  mcp.setThermocoupleType(THERMOCOUPLE_DRIVER_TYPE);
  mcp.setFilterCoefficient(0);
  mcp.enable(true);
  // Allow the first 18-bit conversion and temperature calculation to finish.
  delay(500);
  sensor_ready = true;
}

TempSensorReading temp_get_temperature() {
  if (!sensor_ready) {
    return {NAN, TEMP_SENSOR_INIT_ERROR};
  }
  const uint8_t status = mcp.getStatus();
  Serial.printf("temp_get_temperature status=0x%02x\n", status);
  if (status & (MCP9601_STATUS_OPENCIRCUIT | MCP9601_STATUS_SHORTCIRCUIT)) {
    const bool open = status & MCP9601_STATUS_OPENCIRCUIT;
    const bool shorted = status & MCP9601_STATUS_SHORTCIRCUIT;
    const TempSensorError error = open && shorted ? TEMP_SENSOR_OPEN_AND_SHORT
                                 : (open ? TEMP_SENSOR_OPEN_CIRCUIT : TEMP_SENSOR_SHORT_CIRCUIT);
    Serial.printf("MCP9601 probe fault: status=0x%02x\n", status);
    return {NAN, error};
  }
  const float value = mcp.readThermocouple();
  if (!isfinite(value) || value < -200 || value > 1372) {
    Serial.println("Thermocouple temperature unavailable or outside firmware range");
    return {NAN, TEMP_SENSOR_INVALID_READING};
  }
  return {value, TEMP_SENSOR_OK};
}
