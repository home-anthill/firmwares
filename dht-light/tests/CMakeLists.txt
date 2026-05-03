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
# Test executable: dht_sensor
# ---------------------------------------------------------------------------
add_executable(test_dht_sensor
  test_dht_sensor.cpp
  ../dht_sensor.cpp
)

target_include_directories(test_dht_sensor PRIVATE
  mocks   # Arduino.h, Adafruit_Sensor.h, DHT.h, DHT_U.h stubs
  ..      # dht_sensor.h
)

target_link_libraries(test_dht_sensor
  GTest::gtest_main
)

# ---------------------------------------------------------------------------
# Test executable: light_sensor
# ---------------------------------------------------------------------------
add_executable(test_light_sensor
  test_light_sensor.cpp
  ../light_sensor.cpp
)

target_include_directories(test_light_sensor PRIVATE
  mocks   # Arduino.h, Wire.h, Digital_Light_TSL2561.h stubs
  ..      # light_sensor.h
)

target_link_libraries(test_light_sensor
  GTest::gtest_main
)

# ---------------------------------------------------------------------------
# Test executable: main_ino
#
# <main-project-arduino-file>.ino is compiled as C++ (set_source_files_properties below).
# All external dependencies (wifi_handler, registration, mqtt_handler,
# storage, dht_sensor, light_sensor) are replaced by stubs defined inside
# test_main_ino.cpp itself, so no additional .cpp files are needed.
# setup() and loop() compile and link but are never called by the tests.
# ---------------------------------------------------------------------------
add_executable(test_main_ino
  test_main_ino.cpp
  main_ino_wrapper.cpp   # thin .cpp that #includes <main-project-arduino-file>.ino
)

target_include_directories(test_main_ino PRIVATE
  mocks   # Arduino.h, WiFi.h, WiFiClientSecure.h, PubSubClient.h, TimeAlarms.h stubs
  ..      # wifi_handler.h, mqtt_handler.h, registration.h, storage.h, dht_sensor.h, light_sensor.h, secrets.h
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
add_test(NAME dht_sensor_tests    COMMAND test_dht_sensor)
add_test(NAME light_sensor_tests  COMMAND test_light_sensor)
add_test(NAME main_ino_tests   COMMAND test_main_ino)
