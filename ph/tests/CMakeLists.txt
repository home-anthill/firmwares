cmake_minimum_required(VERSION 3.14)
project(main_tests CXX)

set(HOME_ANTHILL_CORE_SRC "${CMAKE_CURRENT_LIST_DIR}/../../libraries/HomeAnthillCore/src")
include_directories("${HOME_ANTHILL_CORE_SRC}")

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
  ${HOME_ANTHILL_CORE_SRC}/storage.cpp
)

# Mock headers MUST come before any system path so that #include <Arduino.h>
# resolves to our stub and not the real ESP32 SDK (which is not present on
# the host anyway).
target_include_directories(test_storage PRIVATE
  mocks   # provides Arduino.h, Preferences.h stubs
  ..      # local device headers and configuration
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

  ${HOME_ANTHILL_CORE_SRC}/registration.cpp
  ${HOME_ANTHILL_CORE_SRC}/storage.cpp       # shared registration calls storage_set_uuid / storage_set_features
)

target_include_directories(test_registration PRIVATE
  mocks   # Arduino.h, Preferences.h, WiFi.h, WiFiClientSecure.h, HTTPClient.h stubs
  ..      # local device headers and configuration
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

  ${HOME_ANTHILL_CORE_SRC}/mqtt.cpp
  ${HOME_ANTHILL_CORE_SRC}/signing.cpp
)

target_compile_definitions(test_mqtt_handler PRIVATE HOME_ANTHILL_HOST_TEST)

target_include_directories(test_mqtt_handler PRIVATE
  mocks   # Arduino.h, WiFi.h, PubSubClient.h stubs
  ..      # local device headers and configuration
)

target_link_libraries(test_mqtt_handler
  GTest::gtest_main
  ArduinoJson
)

# ---------------------------------------------------------------------------
# Test executable: feature_values
# ---------------------------------------------------------------------------
add_executable(test_feature_values
  test_feature_values.cpp

  ${HOME_ANTHILL_CORE_SRC}/feature_values.cpp
)

target_include_directories(test_feature_values PRIVATE
  mocks   # Arduino.h stubs
  ..      # local device headers and configuration
)

target_link_libraries(test_feature_values
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

target_compile_definitions(test_display PRIVATE HOME_ANTHILL_TEST_OLED_DISPLAY)

target_include_directories(test_display PRIVATE
  mocks   # Arduino.h, Wire.h, Adafruit_GFX.h, Adafruit_SSD1306.h stubs
  ..      # local device headers and configuration
)

target_link_libraries(test_display
  GTest::gtest_main
  ArduinoJson
)

add_executable(test_ph_sensor test_ph_sensor.cpp ../ph_sensor.cpp)
target_include_directories(test_ph_sensor PRIVATE mocks ..)
target_compile_definitions(test_ph_sensor PRIVATE HOME_ANTHILL_TEST_OLED_DISPLAY)
target_link_libraries(test_ph_sensor GTest::gtest_main)

# ---------------------------------------------------------------------------
# Test executable: main_ino
#
# <main-project-arduino-file>.ino is compiled as C++ (set_source_files_properties below).
# All external dependencies (HomeAnthillCore networking and registration,
# storage, ph_sensor, feature_values) are replaced by stubs defined inside
# test_main_ino.cpp itself, so no additional .cpp files are needed.
# setup() and loop() compile and link but are never called by the tests.
# ---------------------------------------------------------------------------
add_executable(test_main_ino
  test_main_ino.cpp
  main_ino_wrapper.cpp   # thin .cpp that #includes <main-project-arduino-file>.ino
)

target_include_directories(test_main_ino PRIVATE
  mocks   # Arduino.h, WiFi.h, WiFiClientSecure.h, PubSubClient.h, TimeAlarms.h stubs
  ..      # local device headers and configuration
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
add_test(NAME feature_values_tests COMMAND test_feature_values)
add_test(NAME display_tests       COMMAND test_display)
add_test(NAME ph_sensor_tests COMMAND test_ph_sensor)
add_test(NAME main_ino_tests   COMMAND test_main_ino)
