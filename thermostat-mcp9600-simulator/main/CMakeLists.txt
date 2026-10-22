# Prepare board configuration for fresh checkouts without overwriting local pins.
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/../config.h")
  configure_file("${CMAKE_CURRENT_LIST_DIR}/../config.h" COPYONLY)
endif()

idf_component_register(
  SRCS "main.c" "display.c"
  INCLUDE_DIRS "." ".."
  REQUIRES esp_driver_gpio esp_driver_i2c esp_timer
)
