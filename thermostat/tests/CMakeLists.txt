cmake_minimum_required(VERSION 3.14)
project(main_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# GoogleTest — fetched at configure time, no system install required.
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
# Prevent GoogleTest from overriding our compiler/linker settings on Windows.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
  arduinojson
  GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
  GIT_TAG        v7.4.2
)
FetchContent_MakeAvailable(arduinojson)

# ---------------------------------------------------------------------------
# Test executable: storage
# ---------------------------------------------------------------------------
add_executable(test_storage
  test_storage.cpp
  ../storage.cpp
)

# Mock headers MUST come before any system path so that #include <Arduino.h>
# resolves to our stub and not the real ESP32 SDK (which is not present on
# the host anyway).
target_include_directories(test_storage PRIVATE
  mocks   # provides Arduino.h, Preferences.h stubs
  ..      # provides storage.h
)

target_link_libraries(test_storage
  GTest::gtest_main
  ArduinoJson
)

# ---------------------------------------------------------------------------
# Test executable: registration
# ---------------------------------------------------------------------------
add_executable(test_registration
  test_registration.cpp
  ../registration.cpp
  ../storage.cpp       # registration.cpp calls storage_set_uuid / storage_set_features
)

target_include_directories(test_registration PRIVATE
  mocks   # Arduino.h, Preferences.h, WiFi.h, WiFiClientSecure.h, HTTPClient.h stubs
  ..      # registration.h, storage.h, secrets.h
)

target_link_libraries(test_registration
  GTest::gtest_main
  ArduinoJson
)

# ---------------------------------------------------------------------------
# Test executable: mqtt_handler
# ---------------------------------------------------------------------------
add_executable(test_mqtt_handler
  test_mqtt_handler.cpp
  ../mqtt_handler.cpp
)

target_compile_definitions(test_mqtt_handler PRIVATE HOME_ANTHILL_HOST_TEST)

target_include_directories(test_mqtt_handler PRIVATE
  mocks   # Arduino.h, WiFi.h, PubSubClient.h stubs
  ..      # mqtt_handler.h, secrets.h
)

target_link_libraries(test_mqtt_handler
  GTest::gtest_main
  ArduinoJson
)

# ---------------------------------------------------------------------------
# Test executable: display
# ---------------------------------------------------------------------------
add_executable(test_display
  test_display.cpp
  ../display.cpp
)

target_include_directories(test_display PRIVATE
  mocks   # Arduino.h, Wire.h, Adafruit_GFX.h, Adafruit_SSD1306.h stubs
  ..      # display.h
)

target_link_libraries(test_display
  GTest::gtest_main
)

# ---------------------------------------------------------------------------
# Test executable: temp_sensor
# ---------------------------------------------------------------------------
add_executable(test_temp_sensor
  test_temp_sensor.cpp
  ../temp_sensor.cpp
)

target_include_directories(test_temp_sensor PRIVATE
  mocks   # Arduino.h, Wire.h, Adafruit_I2CDevice.h, Adafruit_MCP9600.h stubs
  ..      # temp_sensor.h
)

target_link_libraries(test_temp_sensor
  GTest::gtest_main
)

# ---------------------------------------------------------------------------
# Test executable: controller
#
# controller.cpp calls Serial.println(error.f_str()); where error is
# ArduinoJson::DeserializationError.  f_str() is only available when
# ARDUINOJSON_ENABLE_PROGMEM=1.  Setting only this flag (rather than the
# full ARDUINO macro) avoids pulling in Arduino-specific String/Printable
# paths that conflict with the host std::string alias.
# mocks/avr/pgmspace.h satisfies ArduinoJson's <avr/pgmspace.h> include when
# ARDUINO is not defined.
# ---------------------------------------------------------------------------
add_executable(test_controller
  test_controller.cpp
  ../controller.cpp
  ../storage.cpp       # get_setpoint/get_tolerance call storage_get_feature_values
  ../mqtt_handler.cpp
)

target_include_directories(test_controller PRIVATE
  mocks   # Arduino.h, Preferences.h, PubSubClient.h, avr/pgmspace.h stubs
  ..      # controller.h, storage.h, secrets.h
)

target_compile_definitions(test_controller PRIVATE ARDUINOJSON_ENABLE_PROGMEM=1 HOME_ANTHILL_HOST_TEST)

target_link_libraries(test_controller
  GTest::gtest_main
  ArduinoJson
)

# ---------------------------------------------------------------------------
# Test executable: main_ino
#
# thermostat.ino is compiled as C++ via main_ino_wrapper.cpp.
# All external dependencies (wifi_handler, registration, mqtt_handler,
# storage, temp_sensor, display, controller) are replaced by stubs defined
# inside test_main_ino.cpp itself, so no additional .cpp files are needed.
# setup() and loop() compile and link but are never called by the tests.
# ---------------------------------------------------------------------------
add_executable(test_main_ino
  test_main_ino.cpp
  main_ino_wrapper.cpp   # thin .cpp that #includes thermostat.ino
)

target_include_directories(test_main_ino PRIVATE
  mocks   # Arduino.h, WiFi.h, WiFiClientSecure.h, PubSubClient.h, TimeAlarms.h stubs
  ..      # wifi_handler.h, mqtt_handler.h, registration.h, storage.h,
          # temp_sensor.h, display.h, controller.h, secrets.h
)

target_link_libraries(test_main_ino
  GTest::gtest_main
  ArduinoJson
)

# ---------------------------------------------------------------------------
# Register tests with CTest
# ---------------------------------------------------------------------------
enable_testing()
add_test(NAME storage_tests       COMMAND test_storage)
add_test(NAME registration_tests  COMMAND test_registration)
add_test(NAME mqtt_handler_tests  COMMAND test_mqtt_handler)
add_test(NAME display_tests       COMMAND test_display)
add_test(NAME temp_sensor_tests   COMMAND test_temp_sensor)
add_test(NAME controller_tests    COMMAND test_controller)
add_test(NAME main_ino_tests      COMMAND test_main_ino)
