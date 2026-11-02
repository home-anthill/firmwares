cmake_minimum_required(VERSION 3.14)
project(home_anthill_core_tests CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(arduinojson
  GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
  GIT_TAG v7.4.2)
FetchContent_MakeAvailable(googletest arduinojson)

add_executable(test_core_config
  test_core_config.cpp
  ../src/wifi.cpp
  ../src/mqtt.cpp
  ../src/signing.cpp
  ../src/registration.cpp
  ../src/storage.cpp
  ../src/feature_values.cpp)
target_compile_definitions(test_core_config PRIVATE HOME_ANTHILL_HOST_TEST)
target_include_directories(test_core_config PRIVATE mocks ../src)
target_link_libraries(test_core_config GTest::gtest_main ArduinoJson)
enable_testing()
add_test(NAME core_configuration_tests COMMAND test_core_config)
