// Thin C++ wrapper so CMake compiles <main-project-arduino-file>.ino as a regular translation unit.
// The .ino extension is not natively recognised by the host toolchain; wrapping
// it here avoids having to rename or copy the firmware source file.
#include "../dht-light.ino"
