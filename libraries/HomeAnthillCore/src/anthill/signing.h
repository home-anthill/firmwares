#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ha {
// out needs 2 * byte_len + 1 bytes; byte_len must be at most 32.
bool random_hex(char* out, size_t byte_len);
// out needs 65 bytes for a SHA-256 hex digest and its terminator.
bool hmac_sha256_hex(const char* key, const char* message, char* out);
}
