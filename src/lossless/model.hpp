// Adaptive probability models and the binarisation of prediction residuals.
#pragma once

#include <cstdint>
#include <cstdlib>
#include <vector>

#include "lossless/range_coder.hpp"

namespace ic {

// Adaptive probability of a binary event. Two estimates are kept, one that
// adapts quickly and one slowly, and their average is used: this follows
// local changes in the statistics without being too noisy in stable regions.
// The slow estimate starts as a plain average of the bits seen (learning rate
// 1/(n+1.5) after n bits) and settles to the rate 1/(kLimit+1.5), so that rarely
// used contexts learn fast.
struct BitModel {
    uint16_t fast = kProbOne / 2;
    uint16_t slow = kProbOne / 2;
    uint8_t n = 0;

    uint32_t p0() const {
        uint32_t p = (static_cast<uint32_t>(fast) + slow) >> 1;
        if (p < 32) p = 32;
        if (p > kProbOne - 32) p = kProbOne - 32;
        return p;
    }
    void update(int bit) {
        const int32_t target = bit ? 0 : static_cast<int32_t>(kProbOne - 1);
        fast = static_cast<uint16_t>(fast + ((target - fast) >> kFastRate));
        slow = static_cast<uint16_t>(slow + (((target - slow) * recip(n)) >> 16));
        if (n < kLimit) ++n;
    }
    static int32_t recip(int n) { return kRecipTable[n]; }
    struct RecipTable {
        int32_t t[256];
        RecipTable() {
            for (int i = 0; i < 256; ++i) t[i] = static_cast<int32_t>(65536.0 * 2 / (2 * i + 3));
        }
        int32_t operator[](int i) const { return t[i]; }
    };
    static const RecipTable kRecipTable;
    static constexpr int kFastRate = 5;
    static constexpr int kLimit = 254;  // 1/(254+1.5) ~ 2^-8
};

inline const BitModel::RecipTable BitModel::kRecipTable;

// 12-bit probability of a 1 from a BitModel.
inline int p1of(const BitModel& m) {
    int p = static_cast<int>((kProbOne - m.p0()) >> 4);
    return p < 1 ? 1 : (p > 4095 ? 4095 : p);
}

// Codes one bit with an adaptive model (same call for encoder and decoder).
template <class Coder>
inline int codeBit(Coder& c, BitModel& m, int bit) {
    const int b = c.bit(m.p0(), bit);
    m.update(b);
    return b;
}

// ---------------------------------------------------------------------------
// Logistic mixing (as in the PAQ family of compressors). Probabilities are
// combined in the "stretched" domain st(p) = ln(p / (1 - p)), with weights
// learned online. Everything is integer arithmetic with fixed tables, so the
// encoder and the decoder compute exactly the same values on any machine.
// p is a 12-bit probability of a 1 (0..4095); stretched values have 8
// fractional bits and are clamped to [-2047, 2047].

inline int squash(int d) {
    static const int t[33] = {1,    2,    3,    6,    10,   16,   27,   45,   73,   120,  194,
                              310,  488,  747,  1101, 1546, 2047, 2549, 2994, 3348, 3607, 3785,
                              3901, 3975, 4022, 4050, 4068, 4079, 4085, 4089, 4092, 4093, 4094};
    if (d > 2047) return 4095;
    if (d < -2047) return 1;
    const int w = d & 127;
    d = (d >> 7) + 16;
    return (t[d] * (128 - w) + t[d + 1] * w + 64) >> 7;
}

struct StretchTable {
    short t[4096];
    StretchTable() {
        int pi = 0;
        for (int x = -2047; x <= 2047; ++x) {
            const int v = squash(x);
            for (int i = pi; i <= v; ++i) t[i] = static_cast<short>(x);
            pi = v + 1;
        }
        for (int i = pi; i < 4096; ++i) t[i] = 2047;
    }
};
inline const StretchTable kStretch;  // built once at start-up (no check in the hot loop)

inline int stretch(int p) { return kStretch.t[p]; }

// Mixes up to kMax predictions with one of `sets` weight vectors.
class Mixer {
public:
    static constexpr int kMax = 8;
    // rate: learning rate in units of 1/4096 (the palette model uses 24).
    Mixer(int inputs, int sets, int rate, int shift = 12) : n_(inputs), rate_(rate), shift_(shift), w_(static_cast<size_t>(sets) * inputs, (1 << 16) / inputs) {}
    // Sets the initial weight of one input in every set (16 fractional bits).
    void initWeight(int input, int32_t w) {
        for (size_t k = static_cast<size_t>(input); k < w_.size(); k += static_cast<size_t>(n_)) w_[k] = w;
    }

    void add(int st) { x_[k_++] = st; }
    // Mixed 12-bit probability of a 1, with weight set `set`.
    int mix(int set) {
        set_ = set;
        const int32_t* w = &w_[static_cast<size_t>(set) * n_];
        int64_t dot = 0;
        for (int i = 0; i < n_; ++i) dot += static_cast<int64_t>(w[i]) * x_[i];
        int d = static_cast<int>(dot >> 16);
        d = d < -2047 ? -2047 : (d > 2047 ? 2047 : d);
        pr_ = squash(d);
        return pr_;
    }
    void update(int bit) {
        const int err = ((bit << 12) - pr_) * rate_;
        int32_t* w = &w_[static_cast<size_t>(set_) * n_];
        for (int i = 0; i < n_; ++i) w[i] += (x_[i] * err) >> shift_;
        k_ = 0;
    }

private:
    int n_, rate_, shift_, k_ = 0, set_ = 0, pr_ = 2048;
    int x_[kMax] = {};
    std::vector<int32_t> w_;
};

// Converts a 12-bit probability of a 1 to the coder's 16-bit probability of a 0.
inline uint32_t p0from12(int p1) {
    uint32_t p0 = static_cast<uint32_t>(4096 - p1) << 4;
    if (p0 < 32) p0 = 32;
    if (p0 > kProbOne - 32) p0 = kProbOne - 32;
    return p0;
}

// Number of bits needed to write v (bitLength(0) = 0, bitLength(5) = 3).
inline int bitLength(uint32_t v) { return v ? 32 - __builtin_clz(v) : 0; }

// Models used to code the residuals of one channel. A residual e is coded as
//   zero flag   e == 0 ?
//   sign        e > 0 ?
//   class k     bitLength(|e|) - 1, in unary (k ones and a zero)
//   mantissa    the k bits of |e| below the leading one: the first kMantBits
//               are modelled, the rest are sent as equiprobable bits.
// The zero flag and the class bits are predicted by two models with different
// contexts (qa: local activity and recent residuals, with the zero pattern of
// the neighbouring residuals; qb: a second, independent measure of the
// expected error), mixed by logistic mixing; the sign depends on the signs of
// the neighbouring residuals.
class ResidualModel {
public:
    static constexpr int kActivityCtx = 36;
    static constexpr int kCtxB = 32;
    static constexpr int kMaxClass = 17;
    static constexpr int kMantBits = 3;  // modelled mantissa bits (the rest are equiprobable)

    // mix = false (fast profile): only the first model, no mixing
    explicit ResidualModel(int maxAbs, bool mix = true)
        : mix_(mix),
          maxClass_(bitLength(static_cast<uint32_t>(maxAbs)) - 1),
          mixZero_(3, kActivityCtx, mixRate(), 16),
          mixCls_(3, (kMaxClass + 1) * 4, mixRate(), 16) {
        for (Mixer* m : {&mixZero_, &mixCls_}) {
            m->initWeight(0, mixInit());
            m->initWeight(1, (1 << 16) - mixInit());
            m->initWeight(2, 0);
        }
        zero_.resize(kActivityCtx * 9);
        zeroB_.resize(kCtxB);
        sign_.resize(27);
        cls_.resize(kActivityCtx * (kMaxClass + 1));
        clsB_.resize(kCtxB * (kMaxClass + 1));
        mant_.resize((kMaxClass + 1) * 4 * 8);
    }

    // zctx in 0..8 (neighbouring residuals that are zero x fractional part of
    // the prediction), sctx in 0..26,
    // q in 0..kActivityCtx-1, qb in 0..kCtxB-1.
    template <class Coder>
    int code(Coder& c, int e, int q, int zctx, int sctx, int qb) {
        if (mixedBit(c, zero_[q * 9 + zctx], zeroB_[qb], mixZero_, q, e == 0)) return 0;
        const int neg = codeBit(c, sign_[sctx], e < 0);
        const uint32_t mag = static_cast<uint32_t>(e < 0 ? -e : e);  // >= 1

        // class k: bitLength(mag) - 1, unary, the last 1 can be omitted
        const int kEnc = bitLength(mag) - 1;
        int k = 0;
        BitModel* cm = &cls_[q * (kMaxClass + 1)];
        BitModel* cmB = &clsB_[qb * (kMaxClass + 1)];
        const int qs = q >> 3 > 3 ? 3 : q >> 3;
        while (k < maxClass_ && mixedBit(c, cm[k], cmB[k], mixCls_, k * 4 + qs, k < kEnc)) ++k;

        // mantissa: k bits below the leading one
        uint32_t v = 1;
        BitModel* mm = &mant_[k * 32 + (q >> 2 > 3 ? 3 : q >> 2) * 8];
        int i = k - 1;
        for (int j = 0; j < kMantBits && i >= 0; ++j, --i)
            v = (v << 1) | static_cast<uint32_t>(codeBit(c, mm[v & 7], (mag >> i) & 1));
        if (i >= 0) v = (v << (i + 1)) | c.bits(i + 1, mag & ((1u << (i + 1)) - 1));
        return neg ? -static_cast<int>(v) : static_cast<int>(v);
    }

    static int mixRate() { static int r = std::getenv("IC_MIXRATE") ? std::atoi(std::getenv("IC_MIXRATE")) : 8; return r; }
    static int mixInit() { static int r = std::getenv("IC_MIXINIT") ? std::atoi(std::getenv("IC_MIXINIT")) : 32768; return r; }

private:
    // One binary decision predicted by two models, mixed (with a bias input).
    template <class Coder>
    int mixedBit(Coder& c, BitModel& a, BitModel& b, Mixer& mx, int set, int bit) {
        const bool mix = mix_;
        if (!mix) {
            const int r = c.bit(a.p0(), bit);
            a.update(r);
            return r;
        }
        mx.add(stretch(p1of(a)));
        mx.add(stretch(p1of(b)));
        mx.add(256);
        const int p1 = mx.mix(set);
        const int r = c.bit(p0from12(p1), bit);
        mx.update(r);
        a.update(r);
        b.update(r);
        return r;
    }

    bool mix_;
    int maxClass_;
    std::vector<BitModel> zero_, zeroB_, sign_, cls_, clsB_, mant_;
    Mixer mixZero_, mixCls_;
};

}  // namespace ic
