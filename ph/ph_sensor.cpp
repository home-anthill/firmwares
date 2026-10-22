#include "config.h"
#include "secrets.h"
#include "ph_sensor.h"
#include "display.h"

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <Adafruit_ADS1X15.h>
#include <math.h>

constexpr uint8_t PH_ADC_ADDRESS = 0x48;
constexpr float PH_LOW = 4.01f;
constexpr float PH_NEUTRAL = 7.00f;
constexpr float PH_HIGH = 10.01f;
constexpr uint32_t LONG_PRESS_MS = 3000;
constexpr uint32_t DEBOUNCE_MS = 40;
constexpr uint32_t SAMPLE_MS = 250;
constexpr uint32_t CONVERSION_TIMEOUT_MS = 200;
constexpr uint32_t FRESH_SAMPLE_MS = 1500;
constexpr int WINDOW_SIZE = 20;
constexpr float STABLE_RANGE_V = 0.001f;

struct Calibration {
  uint32_t version;
  float ph1;
  float ph2;
  float v1;
  float v2;
};

enum class Mode { MEASURE, SELECT_PAIR, BUFFER_7, BUFFER_SECOND, CONFIRM };

struct PhState {
  Calibration saved = {};
  Calibration pending = {};
  Mode mode = Mode::MEASURE;
  bool calibrated = false;
  bool alkaline_pair = false;
  bool adc_ready = false;
  bool memory_ready = false;
  bool sample_valid = false;
  bool converting = false;
  float volts = NAN;
  float readings[WINDOW_SIZE] = {};
  int count = 0;
  int position = 0;
  bool stable = false;
  uint32_t last_sample = 0;
  uint32_t conversion_started = 0;
  uint32_t last_valid = 0;
  uint32_t last_log = 0;
  bool last_raw = HIGH;
  bool button_state = HIGH;
  bool long_handled = false;
  uint32_t changed_at = 0;
  uint32_t pressed_at = 0;
};

static Adafruit_ADS1115 adc;
static Preferences memory;
static PhState state;

static bool valid_calibration(const Calibration& calibration) {
  if (calibration.version != 1 || !isfinite(calibration.v1) || !isfinite(calibration.v2) ||
      !isfinite(calibration.ph1) || !isfinite(calibration.ph2)) {
    return false;
  }
  if (fabsf(calibration.ph1 - PH_NEUTRAL) > 0.001f) {
    return false;
  }
  if (fabsf(calibration.ph2 - PH_LOW) > 0.001f &&
      fabsf(calibration.ph2 - PH_HIGH) > 0.001f) {
    return false;
  }
  float slope = (calibration.v2 - calibration.v1) / (calibration.ph2 - calibration.ph1);
  return slope < -0.035f && slope > -0.080f;
}

static void reset_stability() {
  state.count = 0;
  state.position = 0;
  state.stable = false;
}

static void invalidate_sample() {
  state.sample_valid = false;
  state.volts = NAN;
  reset_stability();
}

static float average_voltage() {
  float sum = 0;
  for (int i = 0; i < state.count; ++i) {
    sum += state.readings[i];
  }
  return state.count ? sum / state.count : state.volts;
}

PhReading ph_read() {
  bool fresh = state.sample_valid && millis() - state.last_valid <= FRESH_SAMPLE_MS;
  bool calibrating = state.mode != Mode::MEASURE;
  float value = NAN;
  if (fresh && state.calibrated && !calibrating) {
    value = state.saved.ph1 + (state.volts - state.saved.v1) *
        (state.saved.ph2 - state.saved.ph1) / (state.saved.v2 - state.saved.v1);
    if (!isfinite(value) || value < 0 || value > 14) {
      value = NAN;
    }
  }
  return {fresh ? state.volts : NAN, value, state.calibrated,
          fresh && state.stable, calibrating};
}

static void draw_screen() {
  char signal[24];
  char value[24];
  snprintf(signal, sizeof(signal), "Signal: %+.4f V", state.volts);
  switch (state.mode) {
    case Mode::MEASURE: {
      const PhReading reading = ph_read();
      if (!state.adc_ready) {
        display_set_overlay("ADS1115 not found", "Check I2C and power", "Restart after fixing");
      } else if (!state.sample_valid) {
        display_set_overlay("Measurement invalid", "Check probe/wiring", "Hold 3s: calibrate");
      } else if (!state.calibrated) {
        display_set_overlay("Calibration required", signal, "Hold 3s: calibrate");
      } else {
        if (isfinite(reading.ph)) {
          snprintf(value, sizeof(value), "pH: %.2f", reading.ph);
        } else {
          snprintf(value, sizeof(value), "pH out of range");
        }
        display_set_overlay(value, signal, "Hold 3s: calibrate");
      }
      break;
    }
    case Mode::SELECT_PAIR:
      display_set_overlay("Choose buffer pair", state.alkaline_pair ? "7 - 10" : "4 - 7",
                          "Tap: change", "Hold 3s: select");
      break;
    case Mode::BUFFER_7:
    case Mode::BUFFER_SECOND:
      snprintf(value, sizeof(value), "Immerse in pH %.2f",
               state.mode == Mode::BUFFER_7 ? PH_NEUTRAL : state.pending.ph2);
      display_set_overlay(value, signal, state.stable ? "Stable: tap to take" : "Wait for stability",
                          "Hold 3s: cancel");
      break;
    case Mode::CONFIRM:
      display_set_overlay("Save calibration?", "Tap: save", "Hold 3s: cancel");
      break;
  }
}

static void on_long_press() {
  if (state.mode == Mode::MEASURE) {
    state.alkaline_pair = state.calibrated && state.saved.ph2 == PH_HIGH;
    state.mode = Mode::SELECT_PAIR;
    Serial.println("Calibration: choose 4-7 or 7-10.");
  } else if (state.mode == Mode::SELECT_PAIR) {
    state.pending = {1, PH_NEUTRAL, state.alkaline_pair ? PH_HIGH : PH_LOW, 0, 0};
    reset_stability();
    state.mode = Mode::BUFFER_7;
    Serial.println("Rinse probe, then immerse in pH 7.00 buffer.");
  } else {
    state.mode = Mode::MEASURE;
    Serial.println("Calibration cancelled. Previous calibration retained.");
  }
  draw_screen();
}

static void on_short_press() {
  if (state.mode == Mode::SELECT_PAIR) {
    state.alkaline_pair = !state.alkaline_pair;
    Serial.println(state.alkaline_pair ? "Selected buffers: 7.00 / 10.01" : "Selected buffers: 7.00 / 4.01");
  } else if (state.mode == Mode::BUFFER_7 || state.mode == Mode::BUFFER_SECOND) {
    if (!state.stable || !state.sample_valid) {
      Serial.println("Point rejected: wait for a stable signal.");
      return;
    }
    if (state.mode == Mode::BUFFER_7) {
      state.pending.v1 = average_voltage();
      state.mode = Mode::BUFFER_SECOND;
      reset_stability();
      Serial.printf("First point: %.5f V. Rinse, then use pH %.2f buffer.\n",
                    state.pending.v1, state.pending.ph2);
    } else {
      state.pending.v2 = average_voltage();
      if (!valid_calibration(state.pending)) {
        Serial.println("Invalid slope. Check buffers and wiring; take point again.");
        reset_stability();
        display_set_overlay("Invalid calibration", "Check buffers/wiring", "Wait, then retry", "Hold 3s: cancel");
        return;
      }
      state.mode = Mode::CONFIRM;
      Serial.printf("Second point: %.5f V. Tap to save, hold 3s to cancel.\n", state.pending.v2);
    }
  } else if (state.mode == Mode::CONFIRM) {
    if (!state.memory_ready || memory.putBytes("cal", &state.pending, sizeof(state.pending)) != sizeof(state.pending)) {
      Serial.println("Flash write failed. Tap to retry.");
      display_set_overlay("Save failed", "Tap to retry", "Hold 3s: cancel");
      return;
    }
    state.saved = state.pending;
    state.calibrated = true;
    state.mode = Mode::MEASURE;
    Serial.println("Calibration saved to internal flash.");
  }
  draw_screen();
}

static void update_button(uint32_t now) {
  bool raw = digitalRead(PH_CALIBRATION_BUTTON_PIN);
  if (raw != state.last_raw) {
    state.last_raw = raw;
    state.changed_at = now;
  }
  if (now - state.changed_at >= DEBOUNCE_MS && raw != state.button_state) {
    state.button_state = raw;
    if (raw == LOW) {
      state.pressed_at = now;
      state.long_handled = false;
    } else if (!state.long_handled) {
      on_short_press();
    }
  }
  if (state.button_state == LOW && !state.long_handled && now - state.pressed_at >= LONG_PRESS_MS) {
    state.long_handled = true;
    on_long_press();
  }
}

void ph_init_sensor() {
  if (state.memory_ready) {
    memory.end();
  }
  state = PhState{};
  Wire.setPins(I2C_SDA, I2C_SCL);
  Wire.begin();
  Wire.setClock(100000);
  pinMode(PH_CALIBRATION_BUTTON_PIN, INPUT_PULLUP);
  state.adc_ready = adc.begin(PH_ADC_ADDRESS, &Wire);
  if (state.adc_ready) {
    adc.setGain(GAIN_FOUR);
    adc.setDataRate(RATE_ADS1115_16SPS);
  }
  state.memory_ready = memory.begin("ph-meter", false);
  if (state.memory_ready && memory.getBytesLength("cal") == sizeof(state.saved) &&
      memory.getBytes("cal", &state.saved, sizeof(state.saved)) == sizeof(state.saved)) {
    state.calibrated = valid_calibration(state.saved);
  }
  Serial.println(state.calibrated ? "Stored calibration loaded." : "Calibration required.");
  if (!state.memory_ready) {
    Serial.println("pH calibration flash initialization failed.");
  }
  draw_screen();
}

void ph_sensor_update() {
  uint32_t now = millis();
  if (state.sample_valid && now - state.last_valid > FRESH_SAMPLE_MS) {
    invalidate_sample();
  }
  update_button(now);
  if (!state.adc_ready) {
    return;
  }
  // Use the same differential input/gain/rate as the dev-kit reference, but
  // poll completion so a disconnected ADC cannot trap the MQTT loop forever.
  if (!state.converting && now - state.last_sample >= SAMPLE_MS) {
    state.last_sample = now;
    state.conversion_started = now;
    state.converting = true;
    adc.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_0_1, false);
  }
  if (!state.converting) {
    return;
  }
  if (!adc.conversionComplete()) {
    if (now - state.conversion_started > CONVERSION_TIMEOUT_MS) {
      state.converting = false;
      invalidate_sample();
      draw_screen();
    }
    return;
  }
  state.converting = false;
  int16_t raw = adc.getLastConversionResults();
  state.volts = adc.computeVolts(raw);
  if (raw > 32700 || raw < -32700 || !isfinite(state.volts)) {
    invalidate_sample();
    display_set_overlay("ADC out of range", "Check probe/wiring", "Hold 3s: calibrate");
    return;
  }
  state.sample_valid = true;
  state.last_valid = now;
  state.readings[state.position] = state.volts;
  state.position = (state.position + 1) % WINDOW_SIZE;
  if (state.count < WINDOW_SIZE) {
    ++state.count;
  }
  float minimum = state.readings[0];
  float maximum = minimum;
  for (int i = 1; i < state.count; ++i) {
    minimum = fminf(minimum, state.readings[i]);
    maximum = fmaxf(maximum, state.readings[i]);
  }
  state.stable = state.count == WINDOW_SIZE && maximum - minimum <= STABLE_RANGE_V;
  if (now - state.last_log >= 1000) {
    state.last_log = now;
    Serial.printf("ADC=%d voltage=%+.5f V stable=%s pH=%.2f\n",
                  raw, state.volts, state.stable ? "yes" : "no", ph_read().ph);
  }
  draw_screen();
}
