// Binary adaptive arithmetic coder (range coder).
//
// The coder follows the classic LZMA design: a 32-bit range, carry
// propagation through a cached byte, and byte-wise renormalisation. Every
// binary decision is coded with a 16-bit probability (of the bit being 0)
// supplied by an adaptive model (see model.hpp).
//
// RangeEncoder and RangeDecoder expose the same call, bit(p, value): the
// encoder writes `value` and returns it, the decoder ignores `value` and
// returns the decoded bit. The codec is written once as a template over the
// coder type, so the encoder and the decoder always perform exactly the same
// sequence of operations (which is what keeps them synchronised).
#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace ic {

constexpr int kProbBits = 16;
constexpr uint32_t kProbOne = 1u << kProbBits;  // probabilities are in (0, kProbOne)
constexpr uint32_t kTop = 1u << 24;

// Cost (in bits) of coding an event of probability p / kProbOne; only used for
// the statistics printed with -v.
inline double bitCost(uint32_t p) { return -std::log2(static_cast<double>(p) / kProbOne); }

class RangeEncoder {
public:
    static constexpr bool kEncoding = true;

    explicit RangeEncoder(std::vector<uint8_t>& out) : out_(out) {}

    // Codes `bit` with probability p0 (of a 0), p0 in [1, kProbOne - 1].
    int bit(uint32_t p0, int bit) {
        const uint32_t bound = (range_ >> kProbBits) * p0;
        if (stats_) *stats_ += bitCost(bit ? kProbOne - p0 : p0);
        if (bit == 0) {
            range_ = bound;
        } else {
            low_ += bound;
            range_ -= bound;
        }
        while (range_ < kTop) {
            range_ <<= 8;
            shiftLow();
        }
        return bit;
    }

    // Equiprobable bits (no model), most significant first.
    uint32_t bits(int n, uint32_t value) {
        for (int i = n - 1; i >= 0; --i) bit(kProbOne / 2, (value >> i) & 1);
        return value;
    }

    void finish() {
        for (int i = 0; i < 5; ++i) shiftLow();
    }

    // While non-null, the cost of every coded bit is added to *s.
    void setStats(double* s) { stats_ = s; }

private:
    void shiftLow() {
        if (static_cast<uint32_t>(low_) < 0xFF000000u || (low_ >> 32) != 0) {
            const uint8_t carry = static_cast<uint8_t>(low_ >> 32);
            uint8_t temp = cache_;
            do {
                out_.push_back(static_cast<uint8_t>(temp + carry));
                temp = 0xFF;
            } while (--cacheSize_ != 0);
            cache_ = static_cast<uint8_t>(low_ >> 24);
        }
        ++cacheSize_;
        low_ = (low_ & 0x00FFFFFFu) << 8;
    }

    std::vector<uint8_t>& out_;
    uint64_t low_ = 0;
    uint32_t range_ = 0xFFFFFFFFu;
    uint8_t cache_ = 0;
    uint64_t cacheSize_ = 1;
    double* stats_ = nullptr;
};

// Does not produce any output: only adds up the cost (bits) of what would be
// coded. Used by the encoder to compare options on a sample of the image.
class CostCoder {
public:
    static constexpr bool kEncoding = true;
    int bit(uint32_t p0, int bit) {
        static double table[4096];
        static bool init = false;
        if (!init) {
            for (int i = 0; i < 4096; ++i) table[i] = -std::log2((i + 0.5) / 4096.0);
            init = true;
        }
        cost += table[(bit ? kProbOne - p0 : p0) >> 4];
        return bit;
    }
    uint32_t bits(int n, uint32_t value) {
        cost += n;
        return value;
    }
    void setStats(double*) {}
    double cost = 0;
};

class RangeDecoder {
public:
    static constexpr bool kEncoding = false;

    RangeDecoder(const uint8_t* data, size_t size) : p_(data), end_(data + size) {
        for (int i = 0; i < 5; ++i) code_ = (code_ << 8) | nextByte();
    }

    int bit(uint32_t p0, int /*ignored*/) {
        const uint32_t bound = (range_ >> kProbBits) * p0;
        int b;
        if (code_ < bound) {
            range_ = bound;
            b = 0;
        } else {
            code_ -= bound;
            range_ -= bound;
            b = 1;
        }
        while (range_ < kTop) {
            range_ <<= 8;
            code_ = (code_ << 8) | nextByte();
        }
        return b;
    }

    uint32_t bits(int n, uint32_t /*ignored*/) {
        uint32_t v = 0;
        for (int i = 0; i < n; ++i) v = (v << 1) | static_cast<uint32_t>(bit(kProbOne / 2, 0));
        return v;
    }

    void setStats(double*) {}

    // Number of bytes read past the end of the data (a few are normal at the
    // very end; many mean a corrupted or truncated file).
    size_t overrun() const { return overrun_; }

private:
    uint8_t nextByte() {
        if (p_ < end_) return *p_++;
        ++overrun_;
        return 0;
    }

    const uint8_t* p_;
    const uint8_t* end_;
    uint32_t code_ = 0;
    uint32_t range_ = 0xFFFFFFFFu;
    size_t overrun_ = 0;
};

}  // namespace ic
