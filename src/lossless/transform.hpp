// Reversible integer colour transforms for 3-channel images.
//
// The transformed channels are stored interleaved as int32 samples, channel 0
// first (it is coded first and helps predicting the other two). Every
// transform is exactly invertible with integer arithmetic.
//
//   kNone    R, G, B
//   kSubG    G, R-G, B-G
//   kRCT     Y=floor((R+2G+B)/4), R-G, B-G        (JPEG 2000 reversible transform)
//   kYCoCg   Y, Co, Cg                             (YCoCg-R, lifting form)
//   kSubGR   G, R-G, B-(R+G)/2
#pragma once

#include <cstdint>
#include <vector>

namespace ic {

enum ColorTransform : uint8_t { kNone = 0, kSubG = 1, kRCT = 2, kYCoCg = 3, kSubGR = 4, kNumTransforms = 5 };

const char* transformName(int t);

// Range [lo, hi] of each transformed channel when the input is in [0, maxval].
void transformRange(int t, int channels, int maxval, int lo[3], int hi[3]);

// in: interleaved samples (channels per pixel); out: transformed, same layout.
void forwardTransform(int t, int channels, const uint16_t* in, int32_t* out, size_t pixels);
void inverseTransform(int t, int channels, const int32_t* in, uint16_t* out, size_t pixels);

}  // namespace ic
