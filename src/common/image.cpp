#include "image.hpp"

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace ic {

const char* formatName(Format f) {
    switch (f) {
        case Format::RAW: return "RAW";
        case Format::PGM: return "PGM";
        case Format::PPM: return "PPM";
    }
    return "?";
}

int Image::sampleBits() const {
    int b = 0;
    while (b < 16 && (1u << b) <= maxval) ++b;
    return b;
}

std::string Image::describe() const {
    char buf[160];
    std::snprintf(buf, sizeof buf, "%s %dx%d (cols x rows), %d ch, %d bpp, maxval %u%s",
                  formatName(format), width, height, channels, bpp, maxval,
                  (format == Format::RAW && bpp == 16) ? (bigEndian ? ", big-endian" : ", little-endian")
                                                       : "");
    return buf;
}

std::string Image::channelName(int ch) const {
    if (channels == 1) return "Y";
    static const char* names[] = {"R", "G", "B"};
    return ch < 3 ? names[ch] : "C" + std::to_string(ch);
}

// ---------------------------------------------------------------------------
// File helpers

std::vector<uint8_t> readFileBytes(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) throw std::runtime_error("cannot open '" + path + "': " + std::strerror(errno));
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n < 0) {
        std::fclose(f);
        throw std::runtime_error("cannot determine size of '" + path + "'");
    }
    std::vector<uint8_t> buf(static_cast<size_t>(n));
    size_t got = n > 0 ? std::fread(buf.data(), 1, buf.size(), f) : 0;
    std::fclose(f);
    if (got != buf.size()) throw std::runtime_error("error reading '" + path + "'");
    return buf;
}

void writeFileBytes(const std::string& path, const std::vector<uint8_t>& buf) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) throw std::runtime_error("cannot create '" + path + "': " + std::strerror(errno));
    size_t put = std::fwrite(buf.data(), 1, buf.size(), f);
    bool ok = (put == buf.size()) && std::fclose(f) == 0;
    if (!ok) throw std::runtime_error("error writing '" + path + "'");
}

// ---------------------------------------------------------------------------
// PNM parsing

namespace {
struct Cursor {
    const std::vector<uint8_t>& b;
    size_t p = 0;
};

void skipSpaceAndComments(Cursor& c) {
    while (c.p < c.b.size()) {
        uint8_t ch = c.b[c.p];
        if (ch == '#') {
            while (c.p < c.b.size() && c.b[c.p] != '\n' && c.b[c.p] != '\r') ++c.p;
        } else if (std::isspace(ch)) {
            ++c.p;
        } else {
            break;
        }
    }
}

uint32_t readUInt(Cursor& c, const char* what) {
    skipSpaceAndComments(c);
    if (c.p >= c.b.size() || !std::isdigit(c.b[c.p]))
        throw std::runtime_error(std::string("malformed PNM header: expected ") + what);
    uint64_t v = 0;
    while (c.p < c.b.size() && std::isdigit(c.b[c.p])) {
        v = v * 10 + (c.b[c.p] - '0');
        if (v > 0xFFFFFFFFull) throw std::runtime_error(std::string("PNM value too large: ") + what);
        ++c.p;
    }
    return static_cast<uint32_t>(v);
}
}  // namespace

static Image readPNM(const std::vector<uint8_t>& buf, bool withSamples) {
    if (buf.size() < 2 || buf[0] != 'P')
        throw std::runtime_error("not a PGM/PPM file (for RAW images give -rows/-cols)");
    char m = static_cast<char>(buf[1]);
    bool ascii;
    Image img;
    switch (m) {
        case '2': img.format = Format::PGM; ascii = true; break;
        case '5': img.format = Format::PGM; ascii = false; break;
        case '3': img.format = Format::PPM; ascii = true; break;
        case '6': img.format = Format::PPM; ascii = false; break;
        default: throw std::runtime_error(std::string("unsupported PNM type P") + m);
    }
    img.channels = img.format == Format::PPM ? 3 : 1;

    Cursor c{buf, 2};
    uint32_t w = readUInt(c, "width");
    uint32_t h = readUInt(c, "height");
    img.maxval = readUInt(c, "maxval");
    if (w == 0 || h == 0 || w > 1u << 20 || h > 1u << 20)
        throw std::runtime_error("invalid image dimensions");
    if (img.maxval == 0 || img.maxval > 65535) throw std::runtime_error("invalid maxval");
    img.width = static_cast<int>(w);
    img.height = static_cast<int>(h);
    img.bpp = img.maxval < 256 ? 8 : 16;
    img.bigEndian = true;

    const size_t n = img.numSamples();

    if (ascii) {
        img.data.resize(n);
        img.ascii = true;
        for (size_t i = 0; i < n; ++i) {
            uint32_t v = readUInt(c, "sample");
            if (v > img.maxval) throw std::runtime_error("sample exceeds maxval");
            img.data[i] = static_cast<uint16_t>(v);
        }
        return img;
    }

    // Exactly one whitespace character separates the header from the raster.
    if (c.p >= buf.size() || !std::isspace(buf[c.p]))
        throw std::runtime_error("malformed PNM header");
    ++c.p;
    img.rasterOffset = c.p;
    const size_t bps = img.bpp / 8;
    if (buf.size() - c.p < n * bps) throw std::runtime_error("truncated PNM raster");
    if (!withSamples) return img;
    img.data.resize(n);
    const uint8_t* src = buf.data() + c.p;
    if (bps == 1) {
        for (size_t i = 0; i < n; ++i) img.data[i] = src[i];
    } else {
        for (size_t i = 0; i < n; ++i)
            img.data[i] = static_cast<uint16_t>((src[2 * i] << 8) | src[2 * i + 1]);
    }
    return img;
}

static Image readRAW(const std::vector<uint8_t>& buf, const RawParams& raw, bool withSamples) {
    if (raw.rows <= 0 || raw.cols <= 0)
        throw std::runtime_error("RAW images need both -rows and -cols");
    if (raw.bpp != 8 && raw.bpp != 16) throw std::runtime_error("RAW -bpp must be 8 or 16");

    Image img;
    img.format = Format::RAW;
    img.width = raw.cols;
    img.height = raw.rows;
    img.channels = 1;
    img.bpp = raw.bpp;
    img.maxval = (1u << raw.bpp) - 1;
    img.bigEndian = raw.bigEndian;

    const size_t n = img.numSamples();
    const size_t bps = img.bpp / 8;
    if (buf.size() < n * bps)
        throw std::runtime_error("RAW file has " + std::to_string(buf.size()) + " bytes, expected " +
                                 std::to_string(n * bps) + " (check -rows/-cols/-bpp)");
    if (buf.size() > n * bps)
        std::cerr << "warning: RAW file has " << buf.size() - n * bps << " trailing bytes (ignored)\n";

    if (!withSamples) return img;
    img.data.resize(n);
    const uint8_t* src = buf.data();
    if (bps == 1) {
        for (size_t i = 0; i < n; ++i) img.data[i] = src[i];
    } else if (img.bigEndian) {
        for (size_t i = 0; i < n; ++i)
            img.data[i] = static_cast<uint16_t>((src[2 * i] << 8) | src[2 * i + 1]);
    } else {
        for (size_t i = 0; i < n; ++i)
            img.data[i] = static_cast<uint16_t>(src[2 * i] | (src[2 * i + 1] << 8));
    }
    return img;
}

Image parseImage(const std::vector<uint8_t>& file, const RawParams& raw) {
    return raw.given() ? readRAW(file, raw, true) : readPNM(file, true);
}

Image parseImageHeader(const std::vector<uint8_t>& file, const RawParams& raw) {
    return raw.given() ? readRAW(file, raw, false) : readPNM(file, false);
}

Image readImage(const std::string& path, const RawParams& raw) {
    return parseImage(readFileBytes(path), raw);
}

// Appends the binary samples of img to out.
static void appendRaster(const Image& img, std::vector<uint8_t>& out) {
    size_t bps;
    bool be = true;
    if (img.format == Format::RAW) {
        bps = img.bpp / 8;
        be = img.bigEndian;
    } else {
        bps = img.maxval < 256 ? 1 : 2;
    }
    const size_t base = out.size();
    out.resize(base + img.numSamples() * bps);
    uint8_t* dst = out.data() + base;
    const size_t n = img.numSamples();
    if (bps == 1) {
        for (size_t i = 0; i < n; ++i) dst[i] = static_cast<uint8_t>(img.data[i]);
    } else if (be) {
        for (size_t i = 0; i < n; ++i) {
            dst[2 * i] = static_cast<uint8_t>(img.data[i] >> 8);
            dst[2 * i + 1] = static_cast<uint8_t>(img.data[i]);
        }
    } else {
        for (size_t i = 0; i < n; ++i) {
            dst[2 * i] = static_cast<uint8_t>(img.data[i]);
            dst[2 * i + 1] = static_cast<uint8_t>(img.data[i] >> 8);
        }
    }
}

std::vector<uint8_t> encodeRaster(const Image& img) {
    if (img.data.size() != img.numSamples()) throw std::runtime_error("image buffer size mismatch");
    std::vector<uint8_t> out;
    appendRaster(img, out);
    return out;
}

void writeImage(const std::string& path, const Image& img) {
    if (img.data.size() != img.numSamples()) throw std::runtime_error("image buffer size mismatch");
    std::vector<uint8_t> out;
    if (img.format != Format::RAW) {
        char hdr[64];
        int len = std::snprintf(hdr, sizeof hdr, "P%c\n%d %d\n%u\n",
                                img.format == Format::PGM ? '5' : '6', img.width, img.height, img.maxval);
        out.assign(hdr, hdr + len);
    }
    appendRaster(img, out);
    writeFileBytes(path, out);
}

// ---------------------------------------------------------------------------
// Command-line helpers

int parseIntArg(const std::string& flag, const char* value) {
    if (!value) throw std::runtime_error("missing value for " + flag);
    char* end = nullptr;
    errno = 0;
    long v = std::strtol(value, &end, 10);
    if (errno || end == value || *end != '\0' || v < 0 || v > (1L << 30))
        throw std::runtime_error("invalid value for " + flag + ": '" + value + "'");
    return static_cast<int>(v);
}

bool parseRawOption(int argc, char** argv, int& i, RawParams& raw) {
    std::string a = argv[i];
    auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
    if (a == "-rows") raw.rows = parseIntArg(a, next());
    else if (a == "-cols") raw.cols = parseIntArg(a, next());
    else if (a == "-bpp") {
        raw.bpp = parseIntArg(a, next());
        if (raw.bpp != 8 && raw.bpp != 16) throw std::runtime_error("-bpp must be 8 or 16");
    } else if (a == "-be") raw.bigEndian = true;
    else if (a == "-le") raw.bigEndian = false;
    else return false;
    return true;
}

}  // namespace ic
