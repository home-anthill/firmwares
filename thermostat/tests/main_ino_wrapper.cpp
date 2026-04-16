// Thin C++ wrapper so CMake compiles thermostat.ino as a regular translation unit.
// The .ino extension is not natively recognised by the host toolchain; wrapping
// it here avoids having to rename or copy the firmware source file.
#if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32S3)
#define CONFIG_IDF_TARGET_ESP32S2
#endif

#include "../thermostat.ino"
