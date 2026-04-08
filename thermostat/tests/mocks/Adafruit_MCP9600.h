#pragma once

// ---------------------------------------------------------------------------
// Adafruit_MCP9600 mock for host-side (native) unit test compilation.
//
// Captures init calls and provides controllable temperature readings via
// TempSensorMockState so tests can verify temp_sensor.cpp behaviour without
// real I2C hardware.
//
// Usage:
//   TempSensorMockState::reset();
//   TempSensorMockState::instance().thermocouple_value = 23.5f;
//   float t = temp_get_temperature();
//   EXPECT_FLOAT_EQ(t, 23.5f);
// ---------------------------------------------------------------------------

#include <cstdint>
#include <cmath>

// --- Thermocouple types -----------------------------------------------------

typedef enum {
  MCP9600_TYPE_K = 0,
  MCP9600_TYPE_J,
  MCP9600_TYPE_T,
  MCP9600_TYPE_N,
  MCP9600_TYPE_S,
  MCP9600_TYPE_E,
  MCP9600_TYPE_B,
  MCP9600_TYPE_R,
} MCP9600_ThermocoupleType;

// --- ADC resolutions --------------------------------------------------------

typedef enum {
  MCP9600_ADCRESOLUTION_18 = 0,
  MCP9600_ADCRESOLUTION_16,
  MCP9600_ADCRESOLUTION_14,
  MCP9600_ADCRESOLUTION_12,
} MCP9600_ADCResolution;

// --- Ambient resolution -----------------------------------------------------

typedef enum {
  RES_ZERO_POINT_25 = 0,
  RES_ZERO_POINT_125,
  RES_ZERO_POINT_0625,
  RES_ZERO_POINT_03125,
} Ambient_Resolution;

// ---------------------------------------------------------------------------
// Singleton state
// ---------------------------------------------------------------------------

struct TempSensorMockState {
  bool    begin_result{true};
  float   thermocouple_value{22.0f};
  float   ambient_value{20.0f};
  int32_t adc_value{0};
  bool    init_called{false};
  int     read_count{0};

  static TempSensorMockState& instance() {
    static TempSensorMockState s;
    return s;
  }
  static void reset() { instance() = TempSensorMockState{}; }
};

// ---------------------------------------------------------------------------
// Adafruit_MCP9600 mock class
// ---------------------------------------------------------------------------

class Adafruit_MCP9600 {
public:
  bool begin(uint8_t /*addr*/) {
    TempSensorMockState::instance().init_called = true;
    return TempSensorMockState::instance().begin_result;
  }

  void setAmbientResolution(Ambient_Resolution /*res*/)     {}
  void setADCresolution(MCP9600_ADCResolution /*res*/)      {}

  MCP9600_ADCResolution getADCresolution() {
    return MCP9600_ADCRESOLUTION_18;
  }

  void setThermocoupleType(MCP9600_ThermocoupleType /*t*/)  {}

  MCP9600_ThermocoupleType getThermocoupleType() {
    return MCP9600_TYPE_K;
  }

  void    setFilterCoefficient(uint8_t /*coeff*/)           {}
  uint8_t getFilterCoefficient()                            { return 3; }

  void    setAlertTemperature(uint8_t /*n*/, float /*t*/)   {}
  float   getAlertTemperature(uint8_t /*n*/)                { return 30.0f; }

  void    configureAlert(uint8_t /*n*/, bool /*e*/, bool /*r*/) {}
  void    enable(bool /*en*/)                               {}

  float   readThermocouple() {
    TempSensorMockState::instance().read_count++;
    return TempSensorMockState::instance().thermocouple_value;
  }

  float   readAmbient() {
    return TempSensorMockState::instance().ambient_value;
  }

  int32_t readADC() {
    return TempSensorMockState::instance().adc_value;
  }
};
