// img_lossless_enc - lossless compression of RAW/PGM/PPM images (Part II).
//
//   img_lossless_enc [-v] input_img compressed_file                       (PGM/PPM)
//   img_lossless_enc [-v] -rows n -cols n [-bpp 8|16] [-be|-le] input compressed
//
// The decoder (img_lossless_dec) rebuilds a file identical to the input.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "common/image.hpp"
#include "lossless/codec.hpp"

using namespace ic;

static void usage() {
    std::cerr <<
        "Usage: img_lossless_enc [options] input_img compressed_file\n"
        "\n"
        "Options:\n"
        "  -v         print statistics (mode, bits per pixel per channel, time)\n"
        "  -fast      faster encoding and decoding, files ~1-2% larger\n"
        "  -fastest   much faster, files ~5% larger (close to JPEG-LS + colour transform)\n"
        "  -t n       split the image into n stripes coded in parallel (n threads);\n"
        "             files slightly larger; default 1\n"
        "RAW images:\n"
        "  -rows n -cols n [-bpp 8|16] [-be|-le]   (default 8 bpp, big-endian)\n";
}

int main(int argc, char** argv) {
    try {
        RawParams raw;
        std::string in, out;
        bool verbose = false;
        EncodeOptions opt;
        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (parseRawOption(argc, argv, i, raw)) continue;
            if (a == "-h" || a == "--help") { usage(); return 0; }
            if (a == "-v") verbose = true;
            else if (a == "-fast") opt.profile = 1;
            else if (a == "-fastest") opt.profile = 2;
            else if (a == "-t" && i + 1 < argc) {
                opt.threads = std::atoi(argv[++i]);
                if (opt.threads < 1 || opt.threads > 4096) throw std::runtime_error("-t needs a number between 1 and 4096");
            }
            else if (!a.empty() && a[0] == '-') throw std::runtime_error("unknown option " + a);
            else if (in.empty()) in = a;
            else if (out.empty()) out = a;
            else throw std::runtime_error("unexpected argument " + a);
        }
        if (in.empty() || out.empty()) { usage(); return 1; }

        const auto t0 = std::chrono::steady_clock::now();
        const std::vector<uint8_t> file = readFileBytes(in);
        CodecStats st;
        const std::vector<uint8_t> comp = losslessEncode(file, raw, verbose ? &st : nullptr, opt);
        writeFileBytes(out, comp);
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

        if (verbose) {
            std::printf("Input      : %s (%zu bytes)\n", in.c_str(), file.size());
            std::printf("Mode       : %s\n", st.mode.c_str());
            if (!st.description.empty()) std::printf("Image      : %s\n", st.description.c_str());
            std::printf("Output     : %s (%zu bytes, ratio %.3f)\n", out.c_str(), comp.size(),
                        comp.empty() ? 0.0 : static_cast<double>(file.size()) / comp.size());
            if (st.pixels) {
                double tot = 0;
                for (size_t ch = 0; ch < st.channelBits.size(); ++ch) {
                    tot += st.channelBits[ch];
                    std::printf("  channel %zu: %.4f bits/sample\n", ch, st.channelBits[ch] / st.pixels);
                }
                std::printf("  samples  : %.4f bpp (estimated)\n", tot / st.pixels);
                std::printf("  file     : %.4f bpp (with header and trailer)\n", 8.0 * comp.size() / st.pixels);
            }
            std::printf("Time       : %.3f s\n", secs);
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "img_lossless_enc: " << e.what() << "\n";
        return 1;
    }
}
