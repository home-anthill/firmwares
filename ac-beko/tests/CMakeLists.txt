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
# Test executable: feature_values
# ---------------------------------------------------------------------------
add_executable(test_feature_values
  test_feature_values.cpp
  ../feature_values.cpp
)

target_include_directories(test_feature_values PRIVATE
  mocks
  ..
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
  mocks
  ..
)

target_link_libraries(test_display
  GTest::gtest_main
  ArduinoJson
)

# ---------------------------------------------------------------------------
# Test executable: ir_beko_controller
# ---------------------------------------------------------------------------
add_executable(test_ir_beko_controller
  test_ir_beko_controller.cpp
  ../ir_beko_controller.cpp
  ../mqtt_handler.cpp
)

target_include_directories(test_ir_beko_controller PRIVATE
  mocks   # Arduino.h, IRremoteESP8266.h, IRac.h, IRsend.h, ir_Coolix.h, PubSubClient.h stubs
  ..      # ir_beko_controller.h, secrets.h
)

# ARDUINOJSON_ENABLE_PROGMEM=1 enables DeserializationError::f_str(), which
# ir_beko_controller.cpp calls via Serial.println(error.f_str()).  Setting only this flag
# (rather than the full ARDUINO macro) avoids pulling in Arduino-specific
# String/Printable paths that conflict with the host std::string alias.
# Arduino.h already provides the required PROGMEM/pgm_read_byte stubs and
# the __FlashStringHelper forward declaration.
target_compile_definitions(test_ir_beko_controller PRIVATE ARDUINOJSON_ENABLE_PROGMEM=1 HOME_ANTHILL_HOST_TEST)

target_link_libraries(test_ir_beko_controller
  GTest::gtest_main
  ArduinoJson
)

# ---------------------------------------------------------------------------
# Test executable: main_ino
#
# ac-beko.ino is compiled as C++ via main_ino_wrapper.cpp.
# All external dependencies (wifi_handler, registration, mqtt_handler,
# storage, ir_beko_controller) are replaced by stubs defined inside test_main_ino.cpp
# itself, so no additional .cpp files are needed.
# setup() and loop() compile and link but are never called by the tests.
# ---------------------------------------------------------------------------
add_executable(test_main_ino
  test_main_ino.cpp
  main_ino_wrapper.cpp   # thin .cpp that #includes ac-beko.ino
)

target_include_directories(test_main_ino PRIVATE
  mocks   # Arduino.h, WiFi.h, WiFiClientSecure.h, PubSubClient.h, Preferences.h stubs
  ..      # wifi_handler.h, mqtt_handler.h, registration.h, storage.h, ir_beko_controller.h, secrets.h
)

target_link_libraries(test_main_ino
  GTest::gtest_main
  ArduinoJson
)
target_compile_definitions(test_main_ino PRIVATE HOME_ANTHILL_TEST_OLED_DISPLAY)

# ---------------------------------------------------------------------------
# Register tests with CTest
# ---------------------------------------------------------------------------
enable_testing()
add_test(NAME storage_tests       COMMAND test_storage)
add_test(NAME registration_tests  COMMAND test_registration)
add_test(NAME mqtt_handler_tests  COMMAND test_mqtt_handler)
add_test(NAME feature_values_tests COMMAND test_feature_values)
add_test(NAME display_tests       COMMAND test_display)
add_test(NAME ir_beko_tests         COMMAND test_ir_beko_controller)
add_test(NAME main_ino_tests      COMMAND test_main_ino)
