#include "HomeAnthillDisplay.h"
#include <HomeAnthillCore.h>
#include <Adafruit_SSD1306.h>
#include <stdio.h>
#include <string.h>

namespace ha {
  namespace {
    enum class Screen { None, Feature, Message, ConnectionError };

    struct RenderState {
      Screen screen = Screen::None;
      size_t index = 0;
      size_t next_index = 0;
      float value = 0;
      unsigned long shown_at = 0;
    };

    struct ButtonState {
      int last_read = HIGH;
      unsigned long changed_at = 0;
      unsigned long pressed_at = 0;
      bool pressed = false;
      bool long_press_handled = false;
    };

    DisplayConfig config{};
    DisplayPolicy policy{};
    TwoWire* driver_wire = nullptr;
    int8_t driver_reset_pin = -1;
    RenderState render{};
    ButtonState button{};
    bool available = false;
    bool powered = false;
    bool overlay_active = false;
    bool wifi_connected = true;
    bool mqtt_connected = true;
    unsigned long awake_until = 0;
    unsigned long sleep_until = 0;
    unsigned long notice_until = 0;

    /**
     * Returns the single OLED driver, constructed on first use.
     * Its bus object and reset pin come from the first enabled initialization.
     */
    Adafruit_SSD1306& oled() {
      static Adafruit_SSD1306 driver(128, 32, config.wire, config.reset_pin);
      return driver;
    }

    /**
     * Checks whether a nonzero millis() deadline is still pending.
     * Subtraction handles timer rollover; zero means the deadline is inactive.
     */
    bool deadline_active(unsigned long deadline) {
      return deadline != 0 && static_cast<long>(deadline - millis()) > 0;
    }

    /**
     * Copies a unit into a null-terminated buffer, translating the degree symbol
     * from UTF-8 to the OLED font encoding. Capacity must include at least one byte.
     */
    void copy_display_unit(char* output, size_t capacity, const char* unit) {
      size_t written = 0;
      for (size_t i = 0; unit != nullptr && unit[i] != '\0' && written + 1 < capacity; ++i) {
        // The OLED font uses CP437 for the UTF-8 degree symbol.
        if (static_cast<unsigned char>(unit[i]) == 0xC2 &&
            static_cast<unsigned char>(unit[i + 1]) == 0xB0) {
          output[written++] = static_cast<char>(0xF8);
          ++i;
        } else {
          output[written++] = unit[i];
        }
      }
      output[written] = '\0';
    }

    /**
     * Copies the configuration and policy, then clears software state and timers.
     * The static driver and its original wiring are retained.
     */
    void reset(const DisplayConfig& new_config, const DisplayPolicy& new_policy) {
      config = new_config;
      policy = new_policy;
      render = {};
      button = {};
      available = false;
      powered = false;
      overlay_active = false;
      wifi_connected = true;
      mqtt_connected = true;
      awake_until = 0;
      sleep_until = 0;
      notice_until = 0;
    }

    /**
     * Returns whether button support is enabled and a button pin is assigned.
     */
    bool button_enabled() {
      return config.button_enabled && config.button_pin != 255;
    }

    /**
     * Returns whether WiFi or MQTT is currently disconnected.
     */
    bool connection_error() {
      return !wifi_connected || !mqtt_connected;
    }

    /**
     * Returns whether a connection error should be shown instead of feature data.
     * A zero notice duration keeps the error visible until connectivity recovers.
     */
    bool connection_notice_active() {
      return connection_error() &&
          (policy.connectivity_notice_ms == 0 || deadline_active(notice_until));
    }

    /**
     * Applies the firmware visibility callback, or hides only the online feature
     * when no callback is supplied.
     */
    bool feature_visible(const FeatureValue& value) {
      return policy.visible != nullptr ? policy.visible(value) : strcmp(value.name, "online") != 0;
    }

    /**
     * Wakes an initialized, sleeping OLED and invalidates the previous screen.
     */
    void power_on() {
      if (available && !powered) {
        oled().ssd1306_command(SSD1306_DISPLAYON);
        powered = true;
        render.screen = Screen::None;
      }
    }

    /**
     * Clears and powers down the OLED when button-controlled sleep is enabled.
     * Always-on displays are left powered.
     */
    void power_off() {
      if (available && powered && button_enabled()) {
        oled().clearDisplay();
        oled().display();
        oled().ssd1306_command(SSD1306_DISPLAYOFF);
        powered = false;
        render.screen = Screen::None;
      }
    }

    /**
     * Clears the frame buffer and selects white text with the OLED font encoding.
     */
    void begin_frame() {
      oled().clearDisplay();
      oled().setTextColor(WHITE);
      oled().cp437(true);
    }

    /**
     * Draws a small title and a larger value line, then sends the frame to the OLED.
     */
    void draw_lines(const char* title, const char* detail) {
      begin_frame();
      oled().setTextSize(1);
      oled().setCursor(0, 0);
      oled().print(title);
      oled().setTextSize(2);
      oled().setCursor(0, 14);
      oled().print(detail);
      oled().display();
    }

    /**
     * Records the screen type and render time for refresh and message timing.
     */
    void mark_screen(Screen screen) {
      render.screen = screen;
      render.shown_at = millis();
    }

    /**
     * Displays the current WiFi or MQTT failure and records it as an error screen.
     */
    void render_connection_error() {
      draw_lines(wifi_connected ? "MQTT error" : "WiFi status",
                wifi_connected ? "Disconnected" : "Offline");
      mark_screen(Screen::ConnectionError);
    }

    /**
     * Formats and draws one feature using its label, precision, and unit policy.
     * Caches the displayed value so unchanged readings do not need a redraw.
     */
    void render_feature(const FeatureValue& value) {
      char text[32];
      const char* label = policy.label != nullptr ? policy.label(value) : nullptr;
      if (label != nullptr) {
        snprintf(text, sizeof(text), "%s", label);
      } else {
        char unit[16];
        copy_display_unit(unit, sizeof(unit), value.unit);
        bool has_unit = unit[0] != '\0' && strcmp(unit, "-") != 0;
        uint8_t decimals = has_unit ? policy.decimals_with_unit : policy.decimals;
        if (policy.precision != nullptr) {
          decimals = policy.precision(value);
        }
        if (has_unit) {
          snprintf(text, sizeof(text), "%.*f %s", decimals, value.value, unit);
        } else {
          snprintf(text, sizeof(text), "%.*f", decimals, value.value);
        }
      }
      draw_lines(value.name, text);
      render.value = value.value;
      mark_screen(Screen::Feature);
    }

    /**
     * Displays the next visible cached feature that has a value, wrapping at the end.
     * Invalidates the current feature screen if no suitable value is available.
     */
    void render_next_feature() {
      size_t count = feature_values_count();
      for (size_t offset = 0; offset < count; ++offset) {
        // Continue after the current feature and wrap at the end of the cache.
        size_t index = (render.next_index + offset) % count;
        FeatureValue value{};
        if (feature_values_get(index, &value) && value.has_value && feature_visible(value)) {
          render.index = index;
          render.next_index = (index + 1) % count;
          render_feature(value);
          return;
        }
      }
      if (render.screen == Screen::Feature) {
        render.screen = Screen::None;
      }
    }

    /**
     * Redraws the current feature only when its cached value changes.
     * Invalidates it if the feature disappears, loses its value, or becomes hidden.
     */
    void refresh_feature() {
      if (render.screen != Screen::Feature) {
        return;
      }
      FeatureValue value{};
      if (!feature_values_get(render.index, &value) || !value.has_value || !feature_visible(value)) {
        render.screen = Screen::None;
      } else if (value.value != render.value) {
        render_feature(value);
      }
    }

    /**
     * Debounces the active-low button and handles short presses and long holds.
     * A short press wakes and advances the display; a long hold restarts once.
     */
    void handle_button() {
      if (!button_enabled()) {
        return;
      }
      unsigned long now = millis();
      int reading = digitalRead(config.button_pin);
      if (reading != button.last_read) {
        button.last_read = reading;
        button.changed_at = now;
      }
      // Wait for a stable input before accepting a press or release.
      if (now - button.changed_at < config.debounce_ms) {
        return;
      }

      bool pressed = reading == LOW;
      if (pressed && !button.pressed) {
        button.pressed = true;
        button.long_press_handled = false;
        button.pressed_at = now;
      }
      if (pressed && !button.long_press_handled && now - button.pressed_at >= config.long_press_ms) {
        button.long_press_handled = true;
        ESP.restart();
        return;
      }
      if (!pressed && button.pressed) {
        // Releasing a long hold must not also advance to the next feature.
        bool long_press = button.long_press_handled || now - button.pressed_at >= config.long_press_ms;
        button.pressed = false;
        button.long_press_handled = false;
        if (!long_press && available) {
          sleep_until = 0;
          awake_until = now + config.auto_off_ms;
          power_on();
          render_next_feature();
        }
      }
    }

    /**
     * Keeps the display awake during connection failures, overriding requested sleep.
     * Returns to feature data when a finite connection notice expires.
     */
    void update_connection_error() {
      // Connection errors keep the display awake and override requested sleep.
      sleep_until = 0;
      power_on();
      if (connection_notice_active()) {
        if (render.screen != Screen::ConnectionError) {
          render_connection_error();
        }
      } else if (render.screen != Screen::Feature) {
        render_next_feature();
      } else {
        refresh_feature();
      }
    }

    /**
     * Keeps a button-free display awake while respecting temporary message timing.
     * Selects feature data when needed, or redraws it when the policy requests it.
     */
    void update_always_on() {
      sleep_until = 0;
      awake_until = 0;
      power_on();
      if (render.screen == Screen::Message && millis() - render.shown_at < config.persistence_ms) {
        return;
      }
      if (policy.redraw_always_on || render.screen != Screen::Feature) {
        render_next_feature();
      }
    }

    /**
     * Applies requested sleep and the button-controlled wake deadline.
     * While awake, restores feature data after messages and refreshes changed values.
     */
    void update_timed_display() {
      if (deadline_active(sleep_until)) {
        power_off();
        return;
      }
      sleep_until = 0;
      if (!deadline_active(awake_until)) {
        awake_until = 0;
        power_off();
        return;
      }

      power_on();
      if (render.screen == Screen::Message && millis() - render.shown_at >= config.persistence_ms) {
        render_next_feature();
      } else {
        refresh_feature();
      }
    }

  } // namespace

  /**
   * Resets display state and initializes the configured OLED and optional button.
   * Disabled displays perform no hardware operations. Once the driver exists,
   * changing its bus object or reset pin is rejected until reboot.
   */
  void display_init(const DisplayConfig& new_config, const DisplayPolicy& new_policy) {
    reset(new_config, new_policy);
    if (!config.enabled) {
      return;
    }
    if (config.wire == nullptr) {
      Serial.println("Display I2C bus is missing");
      return;
    }

    // The driver is constructed once, using the first enabled configuration.
    if (driver_wire != nullptr &&
        (driver_wire != config.wire || driver_reset_pin != config.reset_pin)) {
      Serial.println("Display bus and reset pin cannot change after initialization");
      return;
    }
    driver_wire = config.wire;
    driver_reset_pin = config.reset_pin;
    if (button_enabled()) {
      pinMode(config.button_pin, INPUT_PULLUP);
      button.last_read = digitalRead(config.button_pin);
    }
    button.changed_at = millis();

    config.wire->setPins(config.sda, config.scl);
    if (!oled().begin(SSD1306_SWITCHCAPVCC, config.address)) {
      Serial.println("SSD1306 allocation failed");
      return;
    }

    available = true;
    powered = true;
    awake_until = button_enabled() ? millis() + config.auto_off_ms : 0;
    begin_frame();
    oled().setTextSize(2);
    oled().setCursor(0, 0);
    oled().print("Starting");
    oled().display();
  }

  /**
   * Services the button and the current connection, always-on, or timed display mode.
   * Disabled displays and active overlays skip normal updates.
   */
  void display_update() {
    if (!config.enabled || overlay_active) {
      return;
    }
    handle_button();
    if (!available) {
      return;
    }

    if (connection_error()) {
      update_connection_error();
    } else if (!button_enabled()) {
      update_always_on();
    } else {
      update_timed_display();
    }
  }

  /**
   * Requests an immediate error or feature redraw and extends the wake deadline.
   * Respects active overlays and requested sleep unless a connection failure overrides it.
   */
  void display_force_update() {
    if (!available || overlay_active) {
      return;
    }
    if (button_enabled() && deadline_active(sleep_until) && !connection_error()) {
      power_off();
      return;
    }

    if (button_enabled()) {
      awake_until = millis() + config.auto_off_ms;
    }
    power_on();
    if (connection_notice_active()) {
      render_connection_error();
    } else {
      render_next_feature();
    }
  }

  /**
   * Wakes the display and shows a temporary two-line message.
   * Null strings use policy defaults; an active overlay keeps control of the screen.
   */
  void display_show_message(const char* title, const char* detail) {
    if (!available || overlay_active) {
      return;
    }
    sleep_until = 0;
    if (button_enabled()) {
      awake_until = millis() + config.auto_off_ms;
    }
    power_on();
    draw_lines(title != nullptr ? title : policy.message_title,
              detail != nullptr ? detail : policy.message_detail);
    mark_screen(Screen::Message);
  }

  /**
   * Stores WiFi and MQTT status, invalidates the screen on changes, and starts
   * or clears the connection-notice deadline. Rendering occurs on a later update.
   */
  void display_set_connectivity_status(bool wifi_online, bool mqtt_online) {
    bool changed = wifi_connected != wifi_online || mqtt_connected != mqtt_online;
    wifi_connected = wifi_online;
    mqtt_connected = mqtt_online;
    if (!config.enabled) {
      return;
    }
    if (changed) {
      render.screen = Screen::None;
    }
    if (!connection_error()) {
      notice_until = 0;
    } else if (changed) {
      notice_until = millis() + policy.connectivity_notice_ms;
    }
  }

  /**
   * Requests a timed sleep for a button-controlled display.
   * Always-on mode ignores the request; connection errors can keep the screen awake.
   */
  void display_sleep_for(unsigned long duration_ms) {
    if (!available) {
      return;
    }
    if (!button_enabled()) {
      sleep_until = 0;
      awake_until = 0;
      power_on();
      return;
    }

    sleep_until = millis() + duration_ms;
    awake_until = 0;
    if (!connection_error()) {
      power_off();
    }
  }

  /**
   * Shows up to four small-text lines and pauses normal rendering and button handling.
   * The overlay remains in control until display_clear_overlay() is called.
   */
  void display_set_overlay(const char* line1, const char* line2, const char* line3, const char* line4) {
    if (!available) {
      return;
    }
    // Calibration overlays own the screen until explicitly cleared.
    overlay_active = true;
    sleep_until = 0;
    power_on();
    begin_frame();
    oled().setTextSize(1);
    const char* lines[] = {line1, line2, line3, line4};
    for (int i = 0; i < 4; ++i) {
      oled().setCursor(0, i * 8);
      oled().print(lines[i] != nullptr ? lines[i] : "");
    }
    oled().display();
  }

  /**
   * Releases the overlay and invalidates the screen for the next normal update.
   */
  void display_clear_overlay() {
    overlay_active = false;
    render.screen = Screen::None;
  }

  #ifdef HOME_ANTHILL_TEST_OLED_DISPLAY
  /**
   * Resets software state for host tests without initializing display hardware.
   * The static driver wiring remains fixed across tests.
   */
  void display_reset_for_test(const DisplayConfig& new_config, const DisplayPolicy& new_policy) {
    reset(new_config, new_policy);
    available = config.enabled;
    powered = config.enabled;
  }

  /**
   * Overrides the availability flag so host tests can simulate an unavailable OLED.
   */
  void display_set_available_for_test(bool value) {
    available = value;
  }

  /**
   * Marks the screen as invalid so host tests can request a fresh render.
   */
  void display_invalidate_render_for_test() {
    render.screen = Screen::None;
  }

  /**
   * Returns the software availability flag for host-test assertions.
   */
  bool display_is_available_for_test() {
    return available;
  }

  /**
   * Returns the tracked power state for host-test assertions.
   */
  bool display_is_powered_for_test() {
    return powered;
  }

  /**
   * Returns the configured button pin, or 255 when button support is disabled.
   */
  uint8_t display_button_pin_for_test() {
    return button_enabled() ? config.button_pin : 255;
  }

  #endif
}
