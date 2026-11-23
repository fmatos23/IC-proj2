// CRC-32 (IEEE 802.3 polynomial, as in ZIP and PNG). Stored in the compressed
// file so that the decoder can check that it rebuilt the original exactly.
#pragma once

#include <cstddef>
#include <cstdint>

namespace ic {

inline uint32_t crc32(const uint8_t* data, size_t n, uint32_t crc = 0) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = true;
    }
    crc = ~crc;
    for (size_t i = 0; i < n; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

}  // namespace ic
