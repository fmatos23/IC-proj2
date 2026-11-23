#include "lossless/transform.hpp"

namespace ic {

namespace {
// Floor division by a power of two for negative numbers too.
inline int32_t floorShift(int32_t v, int s) { return v >> s; }  // arithmetic shift = floor
}  // namespace

const char* transformName(int t) {
    switch (t) {
        case kNone: return "RGB";
        case kSubG: return "G,R-G,B-G";
        case kRCT: return "RCT";
        case kYCoCg: return "YCoCg-R";
        case kSubGR: return "G,R-G,B-(R+G)/2";
    }
    return "?";
}

void transformRange(int t, int channels, int maxval, int lo[3], int hi[3]) {
    for (int c = 0; c < 3; ++c) {
        lo[c] = 0;
        hi[c] = maxval;
    }
    if (channels != 3 || t == kNone) return;
    // differences of two samples span [-maxval, maxval]
    lo[1] = lo[2] = -maxval;
    hi[1] = hi[2] = maxval;
    if (t == kYCoCg) {  // Y in [0, maxval]; Co, Cg in [-maxval, maxval]
        lo[0] = 0;
        hi[0] = maxval;
    }
}

void forwardTransform(int t, int channels, const uint16_t* in, int32_t* out, size_t pixels) {
    const size_t n = pixels * channels;
    if (channels != 3 || t == kNone) {
        for (size_t i = 0; i < n; ++i) out[i] = in[i];
        return;
    }
    for (size_t p = 0; p < pixels; ++p) {
        const int32_t R = in[3 * p], G = in[3 * p + 1], B = in[3 * p + 2];
        int32_t* o = out + 3 * p;
        switch (t) {
            case kSubG:
                o[0] = G; o[1] = R - G; o[2] = B - G;
                break;
            case kRCT:
                o[0] = floorShift(R + 2 * G + B, 2); o[1] = R - G; o[2] = B - G;
                break;
            case kYCoCg: {
                const int32_t co = R - B;
                const int32_t tt = B + floorShift(co, 1);
                const int32_t cg = G - tt;
                o[0] = tt + floorShift(cg, 1); o[1] = co; o[2] = cg;
                break;
            }
            case kSubGR:
                o[0] = G; o[1] = R - G; o[2] = B - floorShift(R + G, 1);
                break;
        }
    }
}

void inverseTransform(int t, int channels, const int32_t* in, uint16_t* out, size_t pixels) {
    const size_t n = pixels * channels;
    if (channels != 3 || t == kNone) {
        for (size_t i = 0; i < n; ++i) out[i] = static_cast<uint16_t>(in[i]);
        return;
    }
    for (size_t p = 0; p < pixels; ++p) {
        const int32_t* s = in + 3 * p;
        int32_t R = 0, G = 0, B = 0;
        switch (t) {
            case kSubG:
                G = s[0]; R = s[1] + G; B = s[2] + G;
                break;
            case kRCT:
                G = s[0] - floorShift(s[1] + s[2], 2); R = s[1] + G; B = s[2] + G;
                break;
            case kYCoCg: {
                const int32_t tt = s[0] - floorShift(s[2], 1);
                G = s[2] + tt;
                B = tt - floorShift(s[1], 1);
                R = B + s[1];
                break;
            }
            case kSubGR:
                G = s[0]; R = s[1] + G; B = s[2] + floorShift(R + G, 1);
                break;
        }
        out[3 * p] = static_cast<uint16_t>(R);
        out[3 * p + 1] = static_cast<uint16_t>(G);
        out[3 * p + 2] = static_cast<uint16_t>(B);
    }
}

}  // namespace ic
