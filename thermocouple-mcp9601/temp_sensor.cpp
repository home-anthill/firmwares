#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP9601.h>
#include <math.h>
#include "config.h"
#include "temp_sensor.h"

static Adafruit_MCP9601 mcp;
static bool sensor_ready = false;

void temp_init_sensor() {
  sensor_ready = false;
  Wire.setPins(I2C_SDA, I2C_SCL);
  if (!mcp.begin(MCP9601_I2C_ADDRESS)) {
    Serial.println("MCP9601 not found or incorrect device identity");
    return;
  }
  mcp.setAmbientResolution(RES_ZERO_POINT_0625);
  mcp.setADCresolution(MCP9600_ADCRESOLUTION_18);
  mcp.setThermocoupleType(MCP9600_TYPE_K);
  mcp.setFilterCoefficient(0);
  mcp.enable(true);
  // Allow the first 18-bit conversion and temperature calculation to finish.
  delay(500);
  sensor_ready = true;
}

float temp_get_temperature() {
  if (!sensor_ready) {
    return NAN;
  }
  const uint8_t status = mcp.getStatus();
  if (status & (MCP9601_STATUS_OPENCIRCUIT | MCP9601_STATUS_SHORTCIRCUIT)) {
    Serial.printf("MCP9601 probe fault: status=0x%02x\n", status);
    return NAN;
  }
  const float value = mcp.readThermocouple();
  if (!isfinite(value) || value < -200 || value > 1372) {
    Serial.println("MCP9601 temperature unavailable or outside type-K range");
    return NAN;
  }
  return value;
}
