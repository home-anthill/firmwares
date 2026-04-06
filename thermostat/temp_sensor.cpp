// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>

// Thermocouple MCP9600
// include specific libraries:
// - OneWire: https://github.com/PaulStoffregen/OneWire
// - DallasTemperature: https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <Adafruit_I2CDevice.h>
#include <Adafruit_I2CRegister.h>
#include "Adafruit_MCP9600.h"

#define I2C_ADDRESS (0x67)
#define I2C_SDA 39
#define I2C_SCL 40
Adafruit_MCP9600 mcp;
/* Set and print ambient resolution */
Ambient_Resolution ambientRes = RES_ZERO_POINT_0625;


void temp_init_sensor() {
  Serial.println("temp_init_sensor - called");
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

  mcp.setThermocoupleType(MCP9600_TYPE_K);
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

  return thermocouple;
}