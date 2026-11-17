// img_quant - uniform scalar quantization of RAW/PGM/PPM images.
//
// Each sample x in [0, M] (M = maxval) is mapped to one of L = 2^b levels:
//     q   = floor(x * L / (M + 1))                       (quantization index)
//     x^  = floor((2q + 1) * (M + 1) / (2L))             (mid-point reconstruction)
// For M = 2^B - 1 this is simply q = x >> (B - b), i.e. dropping the B-b least
// significant bits, and the reconstruction is the centre of each interval.
//
// Output modes:
//   default    the image keeps its original range, with the reconstructed
//              values (directly viewable and comparable with img_cmp);
//   -index     the image stores the indices q, with maxval 2^b - 1, i.e. it
//              literally uses b bits per sample (8-bit container if b <= 8).
//   -floor     reconstruct to the lower end of the interval (q << (B-b))
//              instead of the mid-point.

#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>

#include "common/image.hpp"

using namespace ic;

static void usage() {
    std::cerr <<
        "Usage: img_quant [options] -b bits input_img output_img\n"
        "\n"
        "Options:\n"
        "  -b bits    number of bits per sample after quantization (1..original bits)\n"
        "  -index     write quantization indices (maxval 2^bits-1) instead of\n"
        "             reconstructed values\n"
        "  -floor     reconstruct to the lower end of each interval (default: mid-point)\n"
        "RAW images:\n"
        "  -rows n -cols n [-bpp 8|16] [-be|-le]   (default 8 bpp, big-endian)\n";
}

int main(int argc, char** argv) {
    try {
        RawParams raw;
        std::string input, output;
        int bits = -1;
        bool indexMode = false, floorMode = false;

        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (parseRawOption(argc, argv, i, raw)) continue;
            if (a == "-h" || a == "--help") { usage(); return 0; }
            if (a == "-b") bits = parseIntArg(a, i + 1 < argc ? argv[++i] : nullptr);
            else if (a == "-index") indexMode = true;
            else if (a == "-floor") floorMode = true;
            else if (!a.empty() && a[0] == '-') throw std::runtime_error("unknown option " + a);
            else if (input.empty()) input = a;
            else if (output.empty()) output = a;
            else throw std::runtime_error("unexpected argument " + a);
        }
        if (input.empty() || output.empty() || bits < 0) { usage(); return 1; }

        Image img = readImage(input, raw);
        const int B = img.sampleBits();
        if (bits < 1 || bits > B)
            throw std::runtime_error("-b must be in 1.." + std::to_string(B) + " for this image");

        const uint64_t M1 = static_cast<uint64_t>(img.maxval) + 1;  // number of input levels
        const uint64_t L = 1ull << bits;                            // number of output levels

        // Lookup table: input value -> output value.
        std::vector<uint16_t> lut(M1);
        for (uint64_t x = 0; x < M1; ++x) {
            uint64_t q = x * L / M1;
            uint64_t y;
            if (indexMode) y = q;
            else if (floorMode) y = (q * M1 + L - 1) / L;  // smallest x with this index
            else y = ((2 * q + 1) * M1) / (2 * L);
            if (!indexMode && y > img.maxval) y = img.maxval;
            lut[x] = static_cast<uint16_t>(y);
        }
        for (auto& v : img.data) v = lut[v > img.maxval ? img.maxval : v];

        if (indexMode) {
            img.maxval = static_cast<uint32_t>(L - 1);
            img.bpp = bits <= 8 ? 8 : 16;
        }
        writeImage(output, img);

        std::printf("%s -> %s: %d -> %d bits/sample (%llu levels, step %.3f)%s\n", input.c_str(), output.c_str(),
                    B, bits, (unsigned long long)L, static_cast<double>(M1) / L,
                    indexMode ? ", indices written" : (floorMode ? ", floor reconstruction" : ", mid-point reconstruction"));
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "img_quant: " << e.what() << "\n";
        return 1;
    }
}
