// charls_cli - minimal JPEG-LS command line front-end for the benchmark.
//
// Ubuntu/Debian ship the CharLS library (libcharls-dev) but no command line
// tool, so this wraps it:
//     charls_cli enc input.pgm|ppm output.jls     (binary P5/P6, 8 or 16 bpp)
//     charls_cli dec input.jls output.pgm|ppm
// Lossless (NEAR = 0), standard JPEG-LS with no colour transform; colour images
// use sample interleaving (R G B R G B ...), as stored in PPM.

#include <charls/charls.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> readAll(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error(std::string("cannot open ") + path);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

void writeAll(const char* path, const void* data, size_t n) {
    std::ofstream f(path, std::ios::binary);
    f.write(static_cast<const char*>(data), static_cast<std::streamsize>(n));
    if (!f) throw std::runtime_error(std::string("cannot write ") + path);
}

size_t skipWsComments(const std::vector<uint8_t>& b, size_t p) {
    while (p < b.size()) {
        if (b[p] == '#') {
            while (p < b.size() && b[p] != '\n') ++p;
        } else if (std::isspace(b[p])) {
            ++p;
        } else {
            break;
        }
    }
    return p;
}

unsigned readNum(const std::vector<uint8_t>& b, size_t& p) {
    p = skipWsComments(b, p);
    unsigned v = 0;
    if (p >= b.size() || !std::isdigit(b[p])) throw std::runtime_error("bad PNM header");
    while (p < b.size() && std::isdigit(b[p])) v = v * 10 + (b[p++] - '0');
    return v;
}

int bitsFor(unsigned maxval) {
    int b = 1;
    while ((1u << b) - 1 < maxval) ++b;
    return b < 2 ? 2 : b;  // JPEG-LS needs at least 2 bits
}

int encode(const char* in, const char* out) {
    std::vector<uint8_t> b = readAll(in);
    if (b.size() < 2 || b[0] != 'P' || (b[1] != '5' && b[1] != '6'))
        throw std::runtime_error("only binary PGM (P5) / PPM (P6) supported");
    const int comps = b[1] == '6' ? 3 : 1;
    size_t p = 2;
    unsigned w = readNum(b, p), h = readNum(b, p), maxval = readNum(b, p);
    ++p;  // single whitespace before the raster
    const size_t n = static_cast<size_t>(w) * h * comps;
    const int bps = maxval > 255 ? 2 : 1;
    if (b.size() - p < n * bps) throw std::runtime_error("truncated raster");

    std::vector<uint8_t> raster(b.begin() + static_cast<long>(p), b.begin() + static_cast<long>(p + n * bps));
    if (bps == 2)  // PNM is big-endian, CharLS wants native (little-endian) samples
        for (size_t i = 0; i < n; ++i) std::swap(raster[2 * i], raster[2 * i + 1]);

    charls::jpegls_encoder enc;
    enc.frame_info({w, h, bitsFor(maxval), comps});
    if (comps == 3) enc.interleave_mode(charls::interleave_mode::sample);
    std::vector<uint8_t> dst(enc.estimated_destination_size());
    enc.destination(dst);
    const size_t sz = enc.encode(raster);
    writeAll(out, dst.data(), sz);
    return 0;
}

int decode(const char* in, const char* out) {
    std::vector<uint8_t> src = readAll(in);
    charls::jpegls_decoder dec;
    dec.source(src);
    dec.read_header();
    const charls::frame_info fi = dec.frame_info();
    std::vector<uint8_t> raster(dec.destination_size());
    dec.decode(raster);

    const int bps = fi.bits_per_sample > 8 ? 2 : 1;
    const size_t n = static_cast<size_t>(fi.width) * fi.height * fi.component_count;
    if (bps == 2)
        for (size_t i = 0; i < n; ++i) std::swap(raster[2 * i], raster[2 * i + 1]);
    char hdr[64];
    int len = std::snprintf(hdr, sizeof hdr, "P%c\n%u %u\n%u\n", fi.component_count == 3 ? '6' : '5', fi.width,
                            fi.height, (1u << fi.bits_per_sample) - 1);
    std::vector<uint8_t> outBuf(hdr, hdr + len);
    outBuf.insert(outBuf.end(), raster.begin(), raster.end());
    writeAll(out, outBuf.data(), outBuf.size());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 4 && std::strcmp(argv[1], "enc") == 0) return encode(argv[2], argv[3]);
        if (argc == 4 && std::strcmp(argv[1], "dec") == 0) return decode(argv[2], argv[3]);
        std::cerr << "usage: charls_cli enc in.pgm|ppm out.jls | charls_cli dec in.jls out.pgm|ppm\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "charls_cli: " << e.what() << "\n";
        return 1;
    }
}
