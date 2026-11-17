// Image I/O for RAW, PGM (P2/P5) and PPM (P3/P6) files.
//
// Samples are always stored in memory as uint16_t, interleaved by channel and
// in row-major order, regardless of the on-disk bit depth. This lets every tool
// handle 8 and 16 bpp images with the same code.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ic {

enum class Format { RAW, PGM, PPM };

const char* formatName(Format f);

// Parameters needed to interpret a headerless RAW file.
struct RawParams {
    int rows = 0;
    int cols = 0;
    int bpp = 8;            // 8 or 16
    bool bigEndian = true;  // only meaningful for 16 bpp
    bool given() const { return rows > 0 || cols > 0; }
};

struct Image {
    Format format = Format::PGM;
    int width = 0;
    int height = 0;
    int channels = 1;
    int bpp = 8;             // bits of the on-disk sample container (8 or 16)
    uint32_t maxval = 255;   // largest representable sample value
    bool bigEndian = true;   // byte order of 16 bpp RAW files (PNM is always BE)
    std::vector<uint16_t> data;

    size_t numPixels() const { return static_cast<size_t>(width) * height; }
    size_t numSamples() const { return numPixels() * channels; }

    uint16_t at(int r, int c, int ch) const {
        return data[(static_cast<size_t>(r) * width + c) * channels + ch];
    }
    uint16_t& at(int r, int c, int ch) {
        return data[(static_cast<size_t>(r) * width + c) * channels + ch];
    }

    // Number of significant bits of a sample (e.g. 8 for maxval 255).
    int sampleBits() const;
    // Human readable description: "PPM 512x512, 3 ch, 8 bpp, maxval 255".
    std::string describe() const;
    // Channel names: "Y" for single channel, "R","G","B" for colour.
    std::string channelName(int ch) const;
};

// Reads an image. If raw.given() the file is interpreted as RAW (single
// channel); otherwise the PNM header is parsed.
Image readImage(const std::string& path, const RawParams& raw);

// Writes an image in its own format (img.format). PNM files are written in
// binary form (P5/P6), with big-endian samples when maxval > 255.
void writeImage(const std::string& path, const Image& img);

// Shared command-line handling for the RAW options
//   -rows N  -cols N  -bpp 8|16  -be  -le
// Returns true if argv[i] was one of them (i is advanced past its value).
// Throws std::runtime_error on malformed values.
bool parseRawOption(int argc, char** argv, int& i, RawParams& raw);

// Parses a non-negative integer argument; throws with a helpful message.
int parseIntArg(const std::string& flag, const char* value);

}  // namespace ic
