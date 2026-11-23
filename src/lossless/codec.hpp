// Lossless image codec: public interface used by img_lossless_enc/dec.
//
// Compressed file layout (multi-byte fields little-endian):
//   "ICLL"  magic
//   u8      version (3)
//   u8      mode: 0 = image, 1 = generic bytes (fallback), 2 = stored
//   u64     size of the original file
//   u32     CRC-32 of the original file
//   mode 0:
//     u8 format (0 RAW, 1 PGM, 2 PPM), u8 channels, u8 bpp, u8 big-endian
//     u32 width, u32 height, u32 maxval
//     u32 header length + header bytes   (the original PNM header, verbatim)
//     u64 trailer length                 (bytes after the samples; coded in the stream)
//     u8  colour transform               (see transform.hpp; 0 for grey images)
//     u16 palette size K (0: no palette)
//     u8  x3 gradient scale of each channel (bias contexts)
//     u8  packed values flag (+ u32 minimum, u32 bitmap bytes)
//     u8  profile (0 default, 1 fast, 2 fastest)
//     u8  match model on/off
//     u16 number of stripes S, u32 length of the side stream, u32 x S lengths
//     side stream (range coded): palette entries or bitmap of packed values,
//     then the trailer bytes
//     S stripe streams (range coded): index image or samples of each stripe
//   mode 1: range-coded stream of all the bytes
//   mode 2: the original bytes, stored
//
// The fallback mode is used when the file cannot be rebuilt byte for byte from
// its samples (ASCII PNM, samples above maxval, not an image at all...), so
// that decoding always gives back exactly the original file.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "common/image.hpp"

namespace ic {

struct CodecStats {
    std::string mode;                  // "image" or "bytes" (+ reason)
    std::string description;           // image description
    std::vector<double> channelBits;   // estimated bits spent per channel
    double otherBits = 0;              // trailer / generic bytes
    size_t originalBytes = 0, compressedBytes = 0, pixels = 0;
};

struct EncodeOptions {
    int profile = 0;  // 0: best compression (default), 1: fast, 2: fastest
    int threads = 1;  // > 1: the image is split into this many stripes, coded in parallel
};

// Compresses an image file (RAW needs raw.given()).
std::vector<uint8_t> losslessEncode(const std::vector<uint8_t>& file, const RawParams& raw, CodecStats* stats,
                                    const EncodeOptions& opt = EncodeOptions());

// Rebuilds the original file; throws std::runtime_error on corrupted input.
// threads <= 0: as many as the machine has (only used if the file has stripes).
std::vector<uint8_t> losslessDecode(const std::vector<uint8_t>& comp, CodecStats* stats, int threads = 0);

}  // namespace ic
