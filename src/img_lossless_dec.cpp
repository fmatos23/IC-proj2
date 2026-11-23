// img_lossless_dec - decoder of img_lossless_enc (Part II).
//
//   img_lossless_dec [-v] compressed_file output_img
//
// Everything needed (format, dimensions, byte order, original header) is in
// the compressed file; the output is identical to the encoder's input.

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
        "Usage: img_lossless_dec [options] compressed_file output_img\n"
        "\n"
        "Options:\n"
        "  -v         print statistics\n"
        "  -t n       threads for files with stripes (default: all cores)\n";
}

int main(int argc, char** argv) {
    try {
        std::string in, out;
        bool verbose = false;
        int threads = 0;
        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "-h" || a == "--help") { usage(); return 0; }
            if (a == "-v") verbose = true;
            else if (a == "-t" && i + 1 < argc) threads = std::atoi(argv[++i]);
            else if (!a.empty() && a[0] == '-') throw std::runtime_error("unknown option " + a);
            else if (in.empty()) in = a;
            else if (out.empty()) out = a;
            else throw std::runtime_error("unexpected argument " + a);
        }
        if (in.empty() || out.empty()) { usage(); return 1; }

        const auto t0 = std::chrono::steady_clock::now();
        const std::vector<uint8_t> comp = readFileBytes(in);
        CodecStats st;
        const std::vector<uint8_t> file = losslessDecode(comp, &st, threads);
        writeFileBytes(out, file);
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

        if (verbose) {
            std::printf("Input      : %s (%zu bytes)\n", in.c_str(), comp.size());
            std::printf("Mode       : %s\n", st.mode.c_str());
            if (!st.description.empty()) std::printf("Image      : %s\n", st.description.c_str());
            std::printf("Output     : %s (%zu bytes, CRC OK)\n", out.c_str(), file.size());
            std::printf("Time       : %.3f s\n", secs);
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "img_lossless_dec: " << e.what() << "\n";
        return 1;
    }
}
