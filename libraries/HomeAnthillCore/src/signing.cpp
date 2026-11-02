#include "anthill/signing.h"
#include <cstring>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_random.h>
#include <mbedtls/md.h>
#elif !defined(HOME_ANTHILL_HOST_TEST)
#error "MQTT signing requires ESP32 mbedTLS support"
#endif

namespace ha {

static void bytes_to_hex(const uint8_t* bytes, size_t len, char* out) {
  static const char* hex = "0123456789abcdef";
  for (size_t i = 0; i < len; i++) {
    out[i * 2] = hex[bytes[i] >> 4];
    out[i * 2 + 1] = hex[bytes[i] & 0x0f];
  }
  out[len * 2] = '\0';
}

bool random_hex(char* out, size_t byte_len) {
  uint8_t bytes[32];
  if (byte_len > sizeof(bytes)) {
    out[0] = '\0';
    return false;
  }
#if defined(ARDUINO_ARCH_ESP32)
  esp_fill_random(bytes, byte_len);
#elif defined(HOME_ANTHILL_HOST_TEST)
  // Deterministic host-test nonce only. Production builds never use this branch.
  for (size_t i = 0; i < byte_len; i++) {
    bytes[i] = static_cast<uint8_t>(i);
  }
#else
#error "MQTT nonce generation requires ESP32 entropy"
#endif
  bytes_to_hex(bytes, byte_len, out);
  return true;
}

bool hmac_sha256_hex(const char* key, const char* message, char* out) {
#if defined(ARDUINO_ARCH_ESP32)
  uint8_t digest[32];
  const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (md_info == nullptr || mbedtls_md_hmac(md_info,
                                            reinterpret_cast<const unsigned char*>(key), strlen(key),
                                            reinterpret_cast<const unsigned char*>(message), strlen(message),
                                            digest) != 0) {
    out[0] = '\0';
    return false;
  }
  bytes_to_hex(digest, sizeof(digest), out);
  return true;
#elif defined(HOME_ANTHILL_HOST_TEST)
  // Deterministic host-test signature only. Production builds never use this branch.
  strncpy(out, "aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899", 65);
  return true;
#else
#error "MQTT payload signing requires ESP32 mbedTLS support"
#endif
}

}
