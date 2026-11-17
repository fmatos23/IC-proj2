// img_cmp - compares an image with its original version.
//
// Prints, per channel and globally, the maximum absolute error, the mean
// squared error and the PSNR = 10 log10(peak^2 / MSE), where peak is the
// maxval of the original image (255 for 8-bit images) unless -peak is given.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "common/image.hpp"

using namespace ic;

static void usage() {
    std::cerr <<
        "Usage: img_cmp [options] original_img other_img\n"
        "\n"
        "Options:\n"
        "  -peak v    peak value used in the PSNR (default: maxval of the original)\n"
        "RAW images (applies to both files):\n"
        "  -rows n -cols n [-bpp 8|16] [-be|-le]   (default 8 bpp, big-endian)\n";
}

static std::string psnrStr(double mse, double peak) {
    if (mse == 0.0) return "inf";
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.4f", 10.0 * std::log10(peak * peak / mse));
    return buf;
}

int main(int argc, char** argv) {
    try {
        RawParams raw;
        std::string a1, a2;
        double peak = -1;

        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (parseRawOption(argc, argv, i, raw)) continue;
            if (a == "-h" || a == "--help") { usage(); return 0; }
            if (a == "-peak") peak = parseIntArg(a, i + 1 < argc ? argv[++i] : nullptr);
            else if (!a.empty() && a[0] == '-') throw std::runtime_error("unknown option " + a);
            else if (a1.empty()) a1 = a;
            else if (a2.empty()) a2 = a;
            else throw std::runtime_error("unexpected argument " + a);
        }
        if (a1.empty() || a2.empty()) { usage(); return 1; }

        Image ref = readImage(a1, raw);
        Image img = readImage(a2, raw);
        if (ref.width != img.width || ref.height != img.height || ref.channels != img.channels)
            throw std::runtime_error("images have different dimensions/channels");
        if (ref.maxval != img.maxval)
            std::fprintf(stderr, "warning: different maxval (%u vs %u); comparing raw sample values\n", ref.maxval,
                         img.maxval);
        if (peak <= 0) peak = ref.maxval;

        const int C = ref.channels;
        std::vector<long long> maxErr(C, 0);
        std::vector<double> sse(C, 0.0);
        const size_t n = ref.numSamples();
        for (size_t i = 0; i < n; ++i) {
            int ch = static_cast<int>(i % C);
            long long d = static_cast<long long>(ref.data[i]) - img.data[i];
            long long ad = d < 0 ? -d : d;
            if (ad > maxErr[ch]) maxErr[ch] = ad;
            sse[ch] += static_cast<double>(d * d);
        }

        const double np = static_cast<double>(ref.numPixels());
        std::printf("Original : %s (%s)\n", a1.c_str(), ref.describe().c_str());
        std::printf("Compared : %s\n", a2.c_str());
        std::printf("Peak     : %.0f\n\n", peak);
        std::printf("%-8s %12s %14s %12s\n", "channel", "max |err|", "MSE", "PSNR (dB)");
        long long gMax = 0;
        double gSse = 0;
        for (int ch = 0; ch < C; ++ch) {
            double mse = sse[ch] / np;
            std::printf("%-8s %12lld %14.4f %12s\n", ref.channelName(ch).c_str(), maxErr[ch], mse,
                        psnrStr(mse, peak).c_str());
            gMax = std::max(gMax, maxErr[ch]);
            gSse += sse[ch];
        }
        if (C > 1) {
            double mse = gSse / static_cast<double>(n);
            std::printf("%-8s %12lld %14.4f %12s\n", "all", gMax, mse, psnrStr(mse, peak).c_str());
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "img_cmp: " << e.what() << "\n";
        return 1;
    }
}
