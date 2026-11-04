cmake_minimum_required(VERSION 3.14)
project(home_anthill_display_tests CXX)
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

add_executable(test_display_engine
  test_display_engine.cpp
  ../src/display.cpp
  ../../HomeAnthillCore/src/feature_values.cpp)
target_include_directories(test_display_engine PRIVATE
  mocks ../../HomeAnthillCore/tests/mocks
  ../src ../../HomeAnthillCore/src)
target_compile_definitions(test_display_engine PRIVATE HOME_ANTHILL_TEST_OLED_DISPLAY)
target_link_libraries(test_display_engine GTest::gtest_main ArduinoJson)
enable_testing()
add_test(NAME display_engine_tests COMMAND test_display_engine)
