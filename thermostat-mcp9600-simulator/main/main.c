#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>

#include "driver/gpio.h"
#include "driver/i2c_slave.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "display.h"

#define I2C_PORT I2C_NUM_0
#define I2C_SDA_PIN GPIO_NUM_39
#define I2C_SCL_PIN GPIO_NUM_40
#define I2C_ADDRESS 0x67

#define RGB_LED_PIN GPIO_NUM_48

#define BUTTON_PLUS_01_PIN GPIO_NUM_4
#define BUTTON_PLUS_05_PIN GPIO_NUM_5
#define BUTTON_MINUS_01_PIN GPIO_NUM_6
#define BUTTON_MINUS_05_PIN GPIO_NUM_7

#define BUTTON_DEBOUNCE_MS 30
#define INITIAL_TEMPERATURE_TENTHS 250
#define MIN_TEMPERATURE_TENTHS (-2000)
#define MAX_TEMPERATURE_TENTHS 2000
#define AMBIENT_TEMPERATURE_RAW 400

#define REG_HOT_JUNCTION 0x00
#define REG_JUNCTION_DELTA 0x01
#define REG_COLD_JUNCTION 0x02
#define REG_RAW_ADC 0x03
#define REG_STATUS 0x04
#define REG_SENSOR_CONFIG 0x05
#define REG_DEVICE_CONFIG 0x06
#define REG_ALERT_CONFIG_1 0x08
#define REG_ALERT_HYSTERESIS_1 0x0C
#define REG_ALERT_LIMIT_1 0x10
#define REG_DEVICE_ID 0x20

static const char *TAG = "mcp9600_sim";

typedef struct {
  gpio_num_t pin;
  int16_t delta_tenths;
  bool raw_pressed;
  bool stable_pressed;
  int64_t changed_at_us;
} button_t;

typedef struct {
  uint8_t selected_register;
  int16_t hot_junction_raw;
  uint8_t sensor_config;
  uint8_t device_config;
  uint8_t alert_config[4];
  uint8_t alert_hysteresis[4];
  int16_t alert_limit[4];
} mcp9600_state_t;

static button_t buttons[] = {
    {BUTTON_PLUS_01_PIN, 1, false, false, 0},
    {BUTTON_PLUS_05_PIN, 5, false, false, 0},
    {BUTTON_MINUS_01_PIN, -1, false, false, 0},
    {BUTTON_MINUS_05_PIN, -5, false, false, 0},
};

static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
static mcp9600_state_t sensor = {
    .selected_register = REG_DEVICE_ID,
    .hot_junction_raw = INITIAL_TEMPERATURE_TENTHS * 8 / 5,
};
static int32_t temperature_tenths = INITIAL_TEMPERATURE_TENTHS;
static i2c_slave_dev_handle_t i2c_slave;
static TaskHandle_t response_task_handle;

static int16_t temperature_tenths_to_raw(int32_t value) {
  return (int16_t)lroundf((float)value * 1.6f);
}

static float raw_to_celsius(int16_t raw) {
  return (float)raw * 0.0625f;
}

static void write_register_value_isr(uint8_t reg, const uint8_t *data,
                                     size_t length) {
  if (length == 0) {
    return;
  }

  if (reg == REG_SENSOR_CONFIG) {
    sensor.sensor_config = data[0];
  } else if (reg == REG_DEVICE_CONFIG) {
    // Bit 7 is the MCP9600 software-reset command and self-clears.
    sensor.device_config = data[0] & 0x7f;
  } else if (reg >= REG_ALERT_CONFIG_1 && reg < REG_ALERT_CONFIG_1 + 4) {
    sensor.alert_config[reg - REG_ALERT_CONFIG_1] = data[0];
  } else if (reg >= REG_ALERT_HYSTERESIS_1 &&
             reg < REG_ALERT_HYSTERESIS_1 + 4) {
    sensor.alert_hysteresis[reg - REG_ALERT_HYSTERESIS_1] = data[0];
  } else if (reg >= REG_ALERT_LIMIT_1 && reg < REG_ALERT_LIMIT_1 + 4 &&
             length >= 2) {
    sensor.alert_limit[reg - REG_ALERT_LIMIT_1] =
        (int16_t)(((uint16_t)data[0] << 8) | data[1]);
  }
}

static size_t build_register_response(uint8_t *response) {
  uint8_t reg;
  int16_t hot_junction_raw;
  uint8_t sensor_config;
  uint8_t device_config;
  uint8_t alert_config[4];
  uint8_t alert_hysteresis[4];
  int16_t alert_limit[4];

  portENTER_CRITICAL(&state_lock);
  reg = sensor.selected_register;
  hot_junction_raw = sensor.hot_junction_raw;
  sensor_config = sensor.sensor_config;
  device_config = sensor.device_config;
  for (size_t i = 0; i < 4; ++i) {
    alert_config[i] = sensor.alert_config[i];
    alert_hysteresis[i] = sensor.alert_hysteresis[i];
    alert_limit[i] = sensor.alert_limit[i];
  }
  portEXIT_CRITICAL(&state_lock);

  if (reg == REG_HOT_JUNCTION) {
    response[0] = (uint8_t)((uint16_t)hot_junction_raw >> 8);
    response[1] = (uint8_t)hot_junction_raw;
    return 2;
  }
  if (reg == REG_JUNCTION_DELTA) {
    uint16_t value = (uint16_t)(hot_junction_raw - AMBIENT_TEMPERATURE_RAW);
    response[0] = (uint8_t)(value >> 8);
    response[1] = (uint8_t)value;
    return 2;
  }
  if (reg == REG_COLD_JUNCTION) {
    response[0] = (uint8_t)(AMBIENT_TEMPERATURE_RAW >> 8);
    response[1] = (uint8_t)AMBIENT_TEMPERATURE_RAW;
    return 2;
  }
  if (reg == REG_RAW_ADC) {
    response[0] = 0;
    response[1] = 0;
    response[2] = 0;
    return 3;
  }
  if (reg == REG_STATUS) {
    response[0] = 0x40;
    return 1;
  }
  if (reg == REG_SENSOR_CONFIG) {
    response[0] = sensor_config;
    return 1;
  }
  if (reg == REG_DEVICE_CONFIG) {
    response[0] = device_config;
    return 1;
  }
  if (reg >= REG_ALERT_CONFIG_1 && reg < REG_ALERT_CONFIG_1 + 4) {
    response[0] = alert_config[reg - REG_ALERT_CONFIG_1];
    return 1;
  }
  if (reg >= REG_ALERT_HYSTERESIS_1 && reg < REG_ALERT_HYSTERESIS_1 + 4) {
    response[0] = alert_hysteresis[reg - REG_ALERT_HYSTERESIS_1];
    return 1;
  }
  if (reg >= REG_ALERT_LIMIT_1 && reg < REG_ALERT_LIMIT_1 + 4) {
    uint16_t value = (uint16_t)alert_limit[reg - REG_ALERT_LIMIT_1];
    response[0] = (uint8_t)(value >> 8);
    response[1] = (uint8_t)value;
    return 2;
  }
  if (reg == REG_DEVICE_ID) {
    response[0] = 0x40;
    response[1] = 0x00;
    return 2;
  }

  response[0] = 0;
  return 1;
}

static bool IRAM_ATTR on_i2c_receive(
    i2c_slave_dev_handle_t handle,
    const i2c_slave_rx_done_event_data_t *event,
    void *user_data) {
  (void)handle;
  (void)user_data;

  if (event->buffer == NULL || event->length == 0) {
    return false;
  }

  portENTER_CRITICAL_ISR(&state_lock);
  sensor.selected_register = event->buffer[0];
  write_register_value_isr(sensor.selected_register, event->buffer + 1,
                           event->length - 1);
  portEXIT_CRITICAL_ISR(&state_lock);
  return false;
}

static bool IRAM_ATTR on_i2c_request(
    i2c_slave_dev_handle_t handle,
    const i2c_slave_request_event_data_t *event,
    void *user_data) {
  (void)handle;
  (void)event;
  (void)user_data;

  BaseType_t higher_priority_task_woken = pdFALSE;
  vTaskNotifyGiveFromISR(response_task_handle, &higher_priority_task_woken);
  return higher_priority_task_woken == pdTRUE;
}

static void i2c_response_task(void *argument) {
  (void)argument;
  uint8_t response[3];

  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    size_t response_length = build_register_response(response);
    uint32_t written = 0;
    esp_err_t result = i2c_slave_write(i2c_slave, response, response_length,
                                       &written, 100);
    if (result != ESP_OK || written != response_length) {
      ESP_LOGE(TAG, "I2C response failed: %s, wrote %" PRIu32 "/%u",
               esp_err_to_name(result), written, (unsigned)response_length);
    }
  }
}

static void configure_buttons(void) {
  uint64_t mask = 0;
  for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
    mask |= 1ULL << buttons[i].pin;
  }

  gpio_config_t config = {
      .pin_bit_mask = mask,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&config));
}

static void poll_buttons(void) {
  int64_t now_us = esp_timer_get_time();

  for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
    button_t *button = &buttons[i];
    bool pressed = gpio_get_level(button->pin) == 0;

    if (pressed != button->raw_pressed) {
      button->raw_pressed = pressed;
      button->changed_at_us = now_us;
      ESP_LOGI(TAG, "Button GPIO %d raw state: %s", button->pin,
               pressed ? "PRESSED" : "RELEASED");
    }

    if (button->stable_pressed == button->raw_pressed ||
        now_us - button->changed_at_us < BUTTON_DEBOUNCE_MS * 1000) {
      continue;
    }

    button->stable_pressed = button->raw_pressed;
    if (!button->stable_pressed) {
      continue;
    }

    temperature_tenths += button->delta_tenths;
    if (temperature_tenths < MIN_TEMPERATURE_TENTHS) {
      temperature_tenths = MIN_TEMPERATURE_TENTHS;
    } else if (temperature_tenths > MAX_TEMPERATURE_TENTHS) {
      temperature_tenths = MAX_TEMPERATURE_TENTHS;
    }

    int16_t raw = temperature_tenths_to_raw(temperature_tenths);
    portENTER_CRITICAL(&state_lock);
    sensor.hot_junction_raw = raw;
    portEXIT_CRITICAL(&state_lock);

    simulator_display_update(raw_to_celsius(raw));

    ESP_LOGI(TAG,
             "Current temperature: requested %.1f C, MCP9600 value %.4f C",
             temperature_tenths / 10.0f, raw_to_celsius(raw));
  }
}

static void disable_rgb_led(void) {
  gpio_config_t config = {
      .pin_bit_mask = 1ULL << RGB_LED_PIN,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_ENABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&config));
  ESP_ERROR_CHECK(gpio_set_level(RGB_LED_PIN, 0));
}

static void configure_i2c_slave(void) {
  i2c_slave_config_t config = {
      .i2c_port = I2C_PORT,
      .sda_io_num = I2C_SDA_PIN,
      .scl_io_num = I2C_SCL_PIN,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .send_buf_depth = 64,
      .receive_buf_depth = 64,
      .slave_addr = I2C_ADDRESS,
      .addr_bit_len = I2C_ADDR_BIT_LEN_7,
      .intr_priority = 0,
      .flags = {
          .allow_pd = 0,
          .enable_internal_pullup = 0,
      },
  };
  ESP_ERROR_CHECK(i2c_new_slave_device(&config, &i2c_slave));

  i2c_slave_event_callbacks_t callbacks = {
      .on_request = on_i2c_request,
      .on_receive = on_i2c_receive,
  };
  ESP_ERROR_CHECK(
      i2c_slave_register_event_callbacks(i2c_slave, &callbacks, NULL));
}

void app_main(void) {
  disable_rgb_led();
  configure_buttons();

  BaseType_t task_created = xTaskCreatePinnedToCore(
      i2c_response_task, "mcp9600_response", 4096, NULL,
      configMAX_PRIORITIES - 2, &response_task_handle, 0);
  ESP_ERROR_CHECK(task_created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

  configure_i2c_slave();
  simulator_display_init(raw_to_celsius(sensor.hot_junction_raw));

  ESP_LOGI(TAG, "MCP9600 simulator ready at I2C address 0x%02x", I2C_ADDRESS);
  ESP_LOGI(TAG, "I2C SDA=%d, SCL=%d", I2C_SDA_PIN, I2C_SCL_PIN);
  ESP_LOGI(TAG, "RGB LED disabled on GPIO %d", RGB_LED_PIN);
  ESP_LOGI(TAG, "Buttons connect GPIO 4, 5, 6, or 7 directly to GND");
  ESP_LOGI(TAG, "Current temperature: requested %.1f C, MCP9600 value %.4f C",
           temperature_tenths / 10.0f,
           raw_to_celsius(sensor.hot_junction_raw));

  for (;;) {
    poll_buttons();
    // ESP-IDF defaults to a 100 Hz FreeRTOS tick, so a 5 ms conversion can
    // round down to zero and starve IDLE0 until the task watchdog fires.
    vTaskDelay(1);
  }
}
