#include "display.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"

#define OLED_I2C_PORT I2C_NUM_1
#define OLED_SDA_PIN GPIO_NUM_8
#define OLED_SCL_PIN GPIO_NUM_9
#define OLED_I2C_ADDRESS 0x3c
#define OLED_WIDTH 128
#define OLED_HEIGHT 32
#define OLED_PAGES (OLED_HEIGHT / 8)
#define OLED_TIMEOUT_MS 50

static const char *TAG = "sim_display";

#if CONFIG_MCP9600_SIMULATOR_OLED

static i2c_master_bus_handle_t display_bus;
static i2c_master_dev_handle_t display_device;
static bool display_available;
static uint8_t framebuffer[OLED_WIDTH * OLED_PAGES];

static void disable_after_error(const char *operation, esp_err_t error) {
  if (!display_available) {
    return;
  }

  ESP_LOGW(TAG, "%s failed (%s); disabling optional OLED", operation,
           esp_err_to_name(error));
  display_available = false;
}

static esp_err_t send_commands(const uint8_t *commands, size_t length) {
  uint8_t packet[32];
  if (length + 1 > sizeof(packet)) {
    return ESP_ERR_INVALID_SIZE;
  }

  packet[0] = 0x00;
  memcpy(packet + 1, commands, length);
  return i2c_master_transmit(display_device, packet, length + 1,
                             OLED_TIMEOUT_MS);
}

static esp_err_t send_framebuffer(void) {
  const uint8_t address_window[] = {0x21, 0x00, OLED_WIDTH - 1,
                                    0x22, 0x00, OLED_PAGES - 1};
  esp_err_t result = send_commands(address_window, sizeof(address_window));
  if (result != ESP_OK) {
    return result;
  }

  uint8_t packet[1 + sizeof(framebuffer)];
  packet[0] = 0x40;
  memcpy(packet + 1, framebuffer, sizeof(framebuffer));
  return i2c_master_transmit(display_device, packet, sizeof(packet),
                             OLED_TIMEOUT_MS);
}

static const uint8_t *glyph_for(char character) {
  static const uint8_t blank[5] = {0, 0, 0, 0, 0};
  static const uint8_t zero[5] = {0x3e, 0x51, 0x49, 0x45, 0x3e};
  static const uint8_t one[5] = {0x00, 0x42, 0x7f, 0x40, 0x00};
  static const uint8_t two[5] = {0x42, 0x61, 0x51, 0x49, 0x46};
  static const uint8_t three[5] = {0x21, 0x41, 0x45, 0x4b, 0x31};
  static const uint8_t four[5] = {0x18, 0x14, 0x12, 0x7f, 0x10};
  static const uint8_t five[5] = {0x27, 0x45, 0x45, 0x45, 0x39};
  static const uint8_t six[5] = {0x3c, 0x4a, 0x49, 0x49, 0x30};
  static const uint8_t seven[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
  static const uint8_t eight[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
  static const uint8_t nine[5] = {0x06, 0x49, 0x49, 0x29, 0x1e};
  static const uint8_t minus[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
  static const uint8_t period[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
  static const uint8_t upper_c[5] = {0x3e, 0x41, 0x41, 0x41, 0x22};
  static const uint8_t lower_a[5] = {0x20, 0x54, 0x54, 0x54, 0x78};
  static const uint8_t lower_e[5] = {0x38, 0x54, 0x54, 0x54, 0x18};
  static const uint8_t lower_m[5] = {0x7c, 0x04, 0x18, 0x04, 0x78};
  static const uint8_t lower_p[5] = {0x7c, 0x14, 0x14, 0x14, 0x08};
  static const uint8_t lower_r[5] = {0x7c, 0x08, 0x04, 0x04, 0x08};
  static const uint8_t lower_t[5] = {0x04, 0x3f, 0x44, 0x40, 0x20};
  static const uint8_t lower_u[5] = {0x3c, 0x40, 0x40, 0x20, 0x7c};

  switch (character) {
    case '0': return zero;
    case '1': return one;
    case '2': return two;
    case '3': return three;
    case '4': return four;
    case '5': return five;
    case '6': return six;
    case '7': return seven;
    case '8': return eight;
    case '9': return nine;
    case '-': return minus;
    case '.': return period;
    case 'C': return upper_c;
    case 'a': return lower_a;
    case 'e': return lower_e;
    case 'm': return lower_m;
    case 'p': return lower_p;
    case 'r': return lower_r;
    case 't': return lower_t;
    case 'u': return lower_u;
    default: return blank;
  }
}

static void set_pixel(int x, int y) {
  if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) {
    return;
  }

  framebuffer[x + (y / 8) * OLED_WIDTH] |= 1U << (y % 8);
}

static void draw_character(int x, int y, char character, int scale) {
  const uint8_t *glyph = glyph_for(character);
  for (int column = 0; column < 5; ++column) {
    for (int row = 0; row < 7; ++row) {
      if ((glyph[column] & (1U << row)) == 0) {
        continue;
      }
      for (int dx = 0; dx < scale; ++dx) {
        for (int dy = 0; dy < scale; ++dy) {
          set_pixel(x + column * scale + dx, y + row * scale + dy);
        }
      }
    }
  }
}

static void draw_text(int x, int y, const char *text, int scale) {
  while (*text != '\0' && x < OLED_WIDTH) {
    draw_character(x, y, *text, scale);
    x += 6 * scale;
    ++text;
  }
}

void simulator_display_update(float temperature_celsius) {
  if (!display_available) {
    return;
  }

  char temperature_text[16];
  snprintf(temperature_text, sizeof(temperature_text), "%.2f C",
           temperature_celsius);

  memset(framebuffer, 0, sizeof(framebuffer));
  draw_text(0, 0, "temperature", 1);
  draw_text(0, 14, temperature_text, 2);

  esp_err_t result = send_framebuffer();
  if (result != ESP_OK) {
    disable_after_error("OLED update", result);
  }
}

bool simulator_display_init(float temperature_celsius) {
  i2c_master_bus_config_t bus_config = {
      .i2c_port = OLED_I2C_PORT,
      .sda_io_num = OLED_SDA_PIN,
      .scl_io_num = OLED_SCL_PIN,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .intr_priority = 0,
      .trans_queue_depth = 0,
      .flags = {
          .enable_internal_pullup = 1,
          .allow_pd = 0,
      },
  };

  esp_err_t result = i2c_new_master_bus(&bus_config, &display_bus);
  if (result != ESP_OK) {
    ESP_LOGW(TAG, "Optional OLED bus initialization failed: %s",
             esp_err_to_name(result));
    return false;
  }

  result = i2c_master_probe(display_bus, OLED_I2C_ADDRESS, OLED_TIMEOUT_MS);
  if (result != ESP_OK) {
    ESP_LOGW(TAG,
             "Optional OLED not found at 0x%02x on SDA=%d, SCL=%d; continuing without display",
             OLED_I2C_ADDRESS, OLED_SDA_PIN, OLED_SCL_PIN);
    i2c_del_master_bus(display_bus);
    display_bus = NULL;
    return false;
  }

  i2c_device_config_t device_config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = OLED_I2C_ADDRESS,
      .scl_speed_hz = 400000,
      .scl_wait_us = 0,
      .flags = {
          .disable_ack_check = 0,
      },
  };
  result = i2c_master_bus_add_device(display_bus, &device_config,
                                     &display_device);
  if (result != ESP_OK) {
    ESP_LOGW(TAG, "Optional OLED device initialization failed: %s",
             esp_err_to_name(result));
    i2c_del_master_bus(display_bus);
    display_bus = NULL;
    return false;
  }

  const uint8_t init_commands[] = {
      0xae,       // Display off.
      0xd5, 0x80, // Clock divider.
      0xa8, 0x1f, // Multiplex ratio for 128x32.
      0xd3, 0x00, // Display offset.
      0x40,       // Display start line.
      0x8d, 0x14, // Charge pump on.
      0x20, 0x00, // Horizontal addressing mode.
      0xa1,       // Segment remap.
      0xc8,       // COM scan direction.
      0xda, 0x02, // COM pin configuration for 128x32.
      0x81, 0x8f, // Contrast.
      0xd9, 0xf1, // Pre-charge period.
      0xdb, 0x40, // VCOM detect.
      0xa4,       // Resume RAM display.
      0xa6,       // Normal display.
      0xaf,       // Display on.
  };
  result = send_commands(init_commands, sizeof(init_commands));
  if (result != ESP_OK) {
    ESP_LOGW(TAG, "Optional OLED command initialization failed: %s",
             esp_err_to_name(result));
    i2c_master_bus_rm_device(display_device);
    i2c_del_master_bus(display_bus);
    display_device = NULL;
    display_bus = NULL;
    return false;
  }

  display_available = true;
  ESP_LOGI(TAG, "SSD1306 OLED ready at 0x%02x on SDA=%d, SCL=%d",
           OLED_I2C_ADDRESS, OLED_SDA_PIN, OLED_SCL_PIN);
  simulator_display_update(temperature_celsius);
  return display_available;
}

#else

bool simulator_display_init(float temperature_celsius) {
  (void)temperature_celsius;
  ESP_LOGI(TAG, "Optional OLED disabled by project configuration");
  return false;
}

void simulator_display_update(float temperature_celsius) {
  (void)temperature_celsius;
}

#endif

