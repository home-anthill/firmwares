#pragma once

// ---------------------------------------------------------------------------
// Adafruit_MCP9600 mock for host-side (native) unit test compilation.
//
// Captures init calls and provides controllable temperature readings via
// TempSensorMockState so tests can verify temp_sensor.cpp behaviour without
// real I2C hardware.
//
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
  uint8_t status{0};
  bool    begin_result{true};
  uint8_t device_id{0x41};
  uint8_t expected_device_id{0};
  uint8_t begin_address{0};
  float   thermocouple_value{22.0f};
  float   ambient_value{20.0f};
  int32_t adc_value{0};
  bool    init_called{false};
  int     read_count{0};
  int     set_thermocouple_type_count{0};
  MCP9600_ThermocoupleType thermocouple_type{MCP9600_TYPE_K};

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
  bool begin(uint8_t addr) {
    auto& state = TempSensorMockState::instance();
    state.init_called = true;
    state.begin_address = addr;
    state.expected_device_id = _device_id;
    return state.begin_result && state.device_id == _device_id;
  }

  void setAmbientResolution(Ambient_Resolution /*res*/)     {}
  void setADCresolution(MCP9600_ADCResolution /*res*/)      {}

  MCP9600_ADCResolution getADCresolution() {
    return MCP9600_ADCRESOLUTION_18;
  }

  void setThermocoupleType(MCP9600_ThermocoupleType t)  {
    auto& state = TempSensorMockState::instance();
    state.thermocouple_type = t;
    state.set_thermocouple_type_count++;
  }

  MCP9600_ThermocoupleType getThermocoupleType() {
    return TempSensorMockState::instance().thermocouple_type;
  }

  void    setFilterCoefficient(uint8_t /*coeff*/)           {}
  uint8_t getFilterCoefficient()                            { return 3; }

  void    setAlertTemperature(uint8_t /*n*/, float /*t*/)   {}
  float   getAlertTemperature(uint8_t /*n*/)                { return 30.0f; }

  void    configureAlert(uint8_t /*n*/, bool /*e*/, bool /*r*/) {}
  void    enable(bool /*en*/)                               {}

  uint8_t getStatus() { return TempSensorMockState::instance().status; }

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
protected:
  uint8_t _device_id{0x41};

};
