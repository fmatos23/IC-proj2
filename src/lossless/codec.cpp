#include "lossless/codec.hpp"

#include <algorithm>
#include <atomic>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <cmath>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <stdexcept>

#include "lossless/crc32.hpp"
#include "lossless/model.hpp"
#include "lossless/palette.hpp"
#include "lossless/predictor.hpp"
#include "lossless/range_coder.hpp"
#include "lossless/transform.hpp"

namespace ic {

namespace {

constexpr uint8_t kVersion = 4;
enum Mode : uint8_t { kModeImage = 0, kModeBytes = 1, kModeStored = 2 };

// ---------------------------------------------------------------------------
// Little-endian field I/O for the container header

void put(std::vector<uint8_t>& o, uint64_t v, int bytes) {
    for (int i = 0; i < bytes; ++i) o.push_back(static_cast<uint8_t>(v >> (8 * i)));
}

struct Reader {
    const std::vector<uint8_t>& b;
    size_t p = 0;
    uint64_t get(int bytes) {
        if (b.size() - p < static_cast<size_t>(bytes)) throw std::runtime_error("truncated compressed file");
        uint64_t v = 0;
        for (int i = 0; i < bytes; ++i) v |= static_cast<uint64_t>(b[p++]) << (8 * i);
        return v;
    }
};

// ---------------------------------------------------------------------------
// Generic byte model: order-1 context (previous byte), each byte coded as 8
// binary decisions along a binary tree. Used for the fallback mode and for
// trailing bytes.

class ByteModel {
public:
    ByteModel() : m_(256 * 256) {}

    template <class Coder>
    uint8_t code(Coder& c, uint8_t byte) {
        BitModel* t = &m_[static_cast<size_t>(prev_) * 256];
        uint32_t node = 1;
        for (int i = 7; i >= 0; --i) node = (node << 1) | static_cast<uint32_t>(codeBit(c, t[node], (byte >> i) & 1));
        prev_ = static_cast<uint8_t>(node);
        return prev_;
    }

private:
    std::vector<BitModel> m_;
    uint8_t prev_ = 0;
};

template <class Coder>
void codeBytes(Coder& c, uint8_t* data, size_t n) {
    ByteModel m;
    for (size_t i = 0; i < n; ++i) data[i] = m.code(c, data[i]);
}

// ---------------------------------------------------------------------------
// Image samples. The samples (after the colour transform) are visited in
// raster order and, inside a pixel, channel by channel.
//
// Prediction: a blend of 9 predictors (W, N, W+N-NW, W+NE-N, 2N-NN, two
// adaptive NLMS linear predictors with fast and slow learning rates and, for
// channels 1 and 2, two "same gradient as the previous channel" predictors),
// with weights inversely proportional to the errors each one made at the
// neighbouring positions (W, N, NW, NE). The blend has 3 fractional bits; a
// second-stage NLMS on the neighbouring residuals corrects it and, for
// channels 1 and 2, an adaptive fraction of the channel-0 error is added.
// (pk[2], pk[4], pk[7] and the bias correction are kept for experiments but
// disabled: they stopped helping once the NLMS predictors were added.)
//
// Residual coding (ResidualModel): contexts from the local activity, the
// recent residuals, the expected error of the blend, the residual of channel 0
// and the fractional part of the prediction.
//
// The same function runs in the encoder (v is read) and in the decoder (v is
// written as samples are decoded), so both always do the same operations.

// Tunable parameters (fixed in the codec; overridable through the environment
// variable IC_TUNE="name=value,..." for experiments only).
struct Tune {
    int wErr = 1;       // weight of neighbouring residuals in the context metric
    int wCross = 1;     // weight of channel 0 residual in the context of channels 1, 2
    int icp = 1;        // inter-channel prediction on/off
    int icpDecay = 12;  // memory of the adaptive inter-channel weight (2^-decay)
    int ctxShift = 0;   // extra right shift of the context metric
    int predMask = 0xF6B;  // predictors used in the blend (bit k = predictor k); 5 and 11 did not help
    int signFrac = 1;
    int zeroFrac = 1;
    int fineQ = 1;      // activity context with two levels per octave   // zero-flag context uses the fractional part of the prediction   // sign context uses the fractional part of the prediction
    int qbMode = 0;     // second residual context: 0 blend error (+ch0), 1 + residuals, 2 blend only
    int lmsMu = 1;      // NLMS step 2^-lmsMu
    int mix = 1;        // residual model mixes two models (0: one model)
    int match = 1;      // match model for repeated patterns
    int matchGain = 10; // keep the match model if it saves > matchGain/1000 on the sample
    int matchAlt = 1;   // second candidate after a failed match
    int missModels = 1; // separate residual models after a failed match
    int matchThr = 10;  // code the match decision when it was right > matchThr/16 of the time
    int wSpreadQ = 1;   // same, in the first residual context
    int wSpread = 0;    // weight of the spread of the predictions in the 2nd residual context
    int cascade = 1;    // second-stage NLMS on neighbouring residuals
    int cascMu = 8;
    int cascE0 = 0;
    int lmsMu2 = 3;     // step of the second (slow) NLMS predictor
    int crossRef = 1;   // reference channel of the cross-channel predictors: 0 = channel 0, 1 = previous
    int blendPow = 1;   // weight of a predictor = 1 / (error + 1)^blendPow
    int errNW = 2;      // weight of the NW / NE errors in the blend error (W, N: 2)
    int bias = 0;       // bias correction on/off (made redundant by the residual cascade)
    int biasLimit = 256; // halve the bias statistics after this many samples
    int metric = 0;     // 0: activity + residuals; 1: + blend error; 2: blend error only
    int wBlend = 1;     // weight of the blend error in the metric
    int tex = 3;        // third residual model, context: 0 off, 1 equal neighbours x qb/8, 2 gradients (729), 3 finer gradients (2197)
    int texSign = 1;    // that model also for the sign
    int medC = 0;       // MED predictor also in the blend of channels 1-2
};

Tune loadTune() {
    Tune t;
    if (const char* e = std::getenv("IC_TUNE")) {
        std::string s(e);
        size_t p = 0;
        while (p < s.size()) {
            size_t q = s.find(',', p);
            if (q == std::string::npos) q = s.size();
            std::string kv = s.substr(p, q - p);
            size_t eq = kv.find('=');
            if (eq != std::string::npos) {
                std::string k = kv.substr(0, eq);
                int v = static_cast<int>(std::strtol(kv.c_str() + eq + 1, nullptr, 0));
                if (k == "wErr") t.wErr = v;
                else if (k == "wCross") t.wCross = v;
                else if (k == "icp") t.icp = v;
                else if (k == "icpDecay") t.icpDecay = v;
                else if (k == "ctxShift") t.ctxShift = v;
                else if (k == "predMask") t.predMask = v;
                else if (k == "blendPow") t.blendPow = v;
                else if (k == "errNW") t.errNW = v;
                else if (k == "bias") t.bias = v;
                else if (k == "biasLimit") t.biasLimit = v;
                else if (k == "metric") t.metric = v;
                else if (k == "wBlend") t.wBlend = v;
                else if (k == "tex") t.tex = v;
                else if (k == "texSign") t.texSign = v;
                else if (k == "medC") t.medC = v;
                else if (k == "crossRef") t.crossRef = v;
                else if (k == "lmsMu") t.lmsMu = v;
                else if (k == "lmsMu2") t.lmsMu2 = v;
                else if (k == "cascade") t.cascade = v;
                else if (k == "wSpread") t.wSpread = v;
                else if (k == "wSpreadQ") t.wSpreadQ = v;
                else if (k == "mix") t.mix = v;
                else if (k == "match") t.match = v;
                else if (k == "matchThr") t.matchThr = v;
                else if (k == "missModels") t.missModels = v;
                else if (k == "matchAlt") t.matchAlt = v;
                else if (k == "matchGain") t.matchGain = v;
                else if (k == "cascMu") t.cascMu = v;
                else if (k == "cascE0") t.cascE0 = v;
                else if (k == "qbMode") t.qbMode = v;
                else if (k == "signFrac") t.signFrac = v;
                else if (k == "zeroFrac") t.zeroFrac = v;
                else if (k == "fineQ") t.fineQ = v;
            }
            p = q + 1;
        }
    }
    return t;
}

const Tune& tune() {
    static const Tune t = loadTune();
    return t;
}

// Parameters of a profile: 0 = default (best compression), 1 = fast (no
// mixing in the residual model, no slow NLMS, no residual cascade), 2 =
// fastest (as 1, and only the MED and cross-channel predictors, no NLMS).
Tune profileTune(int profile) {
    Tune t = tune();
    if (profile >= 1) {
        t.mix = 0;
        t.cascade = 0;
        t.predMask &= ~(1 << 11);
    }
    if (profile == 2) {  // fastest: MED (+ the two cross-channel predictors), no NLMS
        t.predMask = (1 << 4) | (1 << 9) | (1 << 10);
    }
    return t;
}

struct Planes {
    int W = 0, H = 0, C = 1;
    int lo[3] = {0, 0, 0}, hi[3] = {0, 0, 0};
    int gshift[3] = {0, 0, 0};  // scale of the gradient quantisation (bias contexts)
    int profile = 0;            // 0 default, 1 fast
    int match = 1;              // match model on (chosen per image by the encoder)
    std::vector<int32_t> v;  // interleaved, W * H * C
};

// Adaptive least-squares weight (fixed point, 8 fractional bits) of the
// channel-0 prediction error when predicting another channel.
struct CrossWeight {
    int64_t num = 0, den = 0;
    int weight() const {
        if (den <= 0) return 0;
        int64_t w = (num * 256) / den;
        return static_cast<int>(w < -512 ? -512 : (w > 512 ? 512 : w));
    }
    void update(int d0, int d, int decay) {
        num += static_cast<int64_t>(d0) * d - (num >> decay);
        den += static_cast<int64_t>(d0) * d0 - (den >> decay);
    }
};

// Bias correction (as in JPEG-LS / CALIC): the mean prediction error is
// tracked in contexts given by the quantised local gradients and subtracted
// from the prediction.
struct BiasTable {
    std::vector<int32_t> sum, cnt;
    explicit BiasTable(size_t n) : sum(n, 0), cnt(n, 0) {}
    int get(size_t k) const { return cnt[k] ? (sum[k] >= 0 ? (sum[k] + cnt[k] / 2) / cnt[k] : -((-sum[k] + cnt[k] / 2) / cnt[k])) : 0; }
    void update(size_t k, int err, int limit) {
        sum[k] += err;
        if (++cnt[k] >= limit) {
            sum[k] /= 2;
            cnt[k] /= 2;
        }
    }
};

// Quantised gradient in -4..4 (thresholds 0, 2, 6, 20, scaled by 2^shift).
inline int quantGrad(int d, int shift) {
    const int a = (d < 0 ? -d : d) >> shift;
    const int q = a == 0 ? 0 : a <= 2 ? 1 : a <= 6 ? 2 : a <= 20 ? 3 : 4;
    return d < 0 ? -q : q;
}

// Table of 2^32 / i for i < 4096 (reciprocals used instead of divisions in
// the per-sample loop).
const uint64_t* recipTable() {
    static uint64_t t[4096];
    static bool init = false;
    if (!init) {
        t[0] = 1ull << 32;
        for (uint64_t i = 1; i < 4096; ++i) t[i] = ((1ull << 32) + i / 2) / i;
        init = true;
    }
    return t;
}
const uint64_t* const kRecip = recipTable();

// Weight 2^32 / v of a predictor with accumulated error v >= 1, from the
// reciprocal of the 12 leading bits of v (8 leading bits were measurably worse).
inline int64_t blendWeight(uint64_t v) {
    if (v < 4096) return static_cast<int64_t>(kRecip[v]);
    const int sh = (64 - __builtin_clzll(v)) - 12;
    return static_cast<int64_t>(kRecip[v >> sh] >> sh);
}

// Adaptive linear predictor trained online with normalised LMS, in integer
// fixed point (weights with 16 fractional bits): predicts x - N from the
// differences between the causal neighbours and N (and, for channels 1-2,
// from the reference channels at the same pixel).
template <int Taps>
struct LMS {
    static constexpr int kTaps = Taps;
    int32_t w[Taps] = {};
    // u: inputs; returns the prediction of x - N with kFP fractional bits
    int64_t predict(const int32_t* u) const {
        int64_t acc = 0;
        for (int k = 0; k < Taps; ++k) acc += static_cast<int64_t>(w[k]) * u[k];
        return acc >> (16 - 3);
    }
    static int64_t norm(const int32_t* u) {
        int64_t n = 16;
        for (int k = 0; k < Taps; ++k) n += static_cast<int64_t>(u[k]) * u[k];
        return n;
    }
    // e8: error with 3 fractional bits; nrm = norm(u)
    void update(const int32_t* u, int64_t e8, int64_t nrm, int muShift) {
        // F ~ e8 * 2^30 / nrm, with a reciprocal instead of a division
        const int lb = 64 - __builtin_clzll(static_cast<uint64_t>(nrm));
        const int rs = lb > 12 ? lb - 12 : 0;
        const int64_t F = (e8 * static_cast<int64_t>(kRecip[nrm >> rs])) >> (2 + rs);
        const int sh = 17 + muShift;
        for (int k = 0; k < Taps; ++k) {
            int64_t nw = w[k] + ((F * u[k]) >> sh);
            nw = nw < -(1 << 18) ? -(1 << 18) : nw;
            w[k] = static_cast<int32_t>(nw > (1 << 18) ? (1 << 18) : nw);
        }
    }
};

// The two spatial NLMS predictors (fast and slow) share their inputs, so they
// are evaluated and updated in the same loops (same results as two LMS<12>).
struct LMSPair {
    static constexpr int kTaps = 12;
    int32_t w1[kTaps] = {32768, -16384, 16384};  // start close to a planar predictor
    int32_t w2[kTaps] = {32768, -16384, 16384};
    void predict(const int32_t* u, int64_t& p1, int64_t& p2) const {
        int64_t a1 = 0, a2 = 0;
        for (int k = 0; k < kTaps; ++k) {
            a1 += static_cast<int64_t>(w1[k]) * u[k];
            a2 += static_cast<int64_t>(w2[k]) * u[k];
        }
        p1 = a1 >> (16 - 3);
        p2 = a2 >> (16 - 3);
    }
    static int64_t factor(int64_t e8, int64_t nrm) {
        const int lb = 64 - __builtin_clzll(static_cast<uint64_t>(nrm));
        const int rs = lb > 12 ? lb - 12 : 0;
        return (e8 * static_cast<int64_t>(kRecip[nrm >> rs])) >> (2 + rs);
    }
    void update(const int32_t* u, int64_t e1, int64_t e2, int mu1, int mu2, bool on1, bool on2) {
        int64_t nrm = 16;
        for (int k = 0; k < kTaps; ++k) nrm += static_cast<int64_t>(u[k]) * u[k];
        const int64_t F1 = on1 ? factor(e1, nrm) : 0, F2 = on2 ? factor(e2, nrm) : 0;
        const int s1 = 17 + mu1, s2 = 17 + mu2;
        for (int k = 0; k < kTaps; ++k) {
            int64_t a = w1[k] + ((F1 * u[k]) >> s1);
            int64_t b = w2[k] + ((F2 * u[k]) >> s2);
            a = a < -(1 << 18) ? -(1 << 18) : (a > (1 << 18) ? (1 << 18) : a);
            b = b < -(1 << 18) ? -(1 << 18) : (b > (1 << 18) ? (1 << 18) : b);
            w1[k] = static_cast<int32_t>(a);
            w2[k] = static_cast<int32_t>(b);
        }
    }
};
using ResidualLMS = LMS<6>;

constexpr int kNumPred = 12;
constexpr int kFP = 3;  // predictions have 3 fractional bits

// Match model, for synthetic images (text, charts, screenshots) made of
// repeated patterns: a hash table remembers, for each context of exact
// neighbouring pixels, the pixel that followed it last time and how many times
// in a row that prediction was right. When the current context is in the
// table, one binary decision says whether the pixel is that one; if so, the
// whole pixel is known and its residuals are not coded. Two contexts are used:
// (W, N, NW, NE), (+ WW, NN, NNE) and (+ WWW, NWW, NEE, NNW, NNEE).
// Entry (64 bits): 3 x 17 bits value - lo per channel, 5 bits count, 8 bits check.
class MatchModel {
public:
    static constexpr int kOrders = 3;
    MatchModel(size_t pixels, int C, const int* lo) : C_(C) {
        bits_ = std::min(20, std::max(12, bitLength(static_cast<uint32_t>(std::min<size_t>(pixels, 1u << 30)))));
        table_.assign(static_cast<size_t>(1) << bits_, 0);
        alt_.assign(static_cast<size_t>(1) << bits_, 0);
        for (int ch = 0; ch < 3; ++ch) lo_[ch] = ch < C ? lo[ch] : 0;
        hit_.resize(kOrders * 32 * 2 * 2);
    }
    // Looks up the contexts, longest first; returns the order found or -1.
    int lookup(const uint64_t* h3, int32_t* pred, int& count, int32_t* alt) {
        hasAlt_ = false;
        for (int k = 0; k < kOrders; ++k) h_[k] = h3[k];
        for (int order = kOrders - 1; order >= 0; --order) {
            const uint64_t h = h_[order];
            const uint64_t e = table_[h >> (64 - bits_)];
            const int n = static_cast<int>((e >> 8) & 31);
            if (n > 0 && (e & 0xFF) == ((h >> 20) & 0xFF)) {
                for (int ch = 0; ch < C_; ++ch) pred[ch] = static_cast<int32_t>((e >> (13 + 17 * ch)) & 0x1FFFF) + lo_[ch];
                count = n;
                // second candidate: the value that this context had before
                const uint64_t a = alt_[h >> (64 - bits_)];
                hasAlt_ = (a & 0xFF) == (e & 0xFF) && ((a >> 8) & 31) > 0;
                if (hasAlt_)
                    for (int ch = 0; ch < C_; ++ch) alt[ch] = static_cast<int32_t>((a >> (13 + 17 * ch)) & 0x1FFFF) + lo_[ch];
                return order;
            }
        }
        return -1;
    }
    BitModel& hitModel(int order, int count, int prevHit, int otherAgrees) {
        return hit_[((order * 32 + count) * 2 + prevHit) * 2 + otherAgrees];
    }
    // The decision is only coded in (order, count) classes where the
    // prediction has been right often enough; otherwise the pixel is coded
    // normally (the statistics are kept in both cases, from the actual pixel).
    bool enabled(int order, int count, int thr16) const {
        const Stat& st = stat_[order * 32 + count];
        return st.total >= 8 && st.hits * 16 > st.total * thr16;
    }
    void record(int order, int count, bool wouldHit) {
        Stat& st = stat_[order * 32 + count];
        st.hits += wouldHit;
        if (++st.total >= 1024) {
            st.hits >>= 1;
            st.total >>= 1;
        }
    }
    // Stores the actual pixel under both contexts.
    void update(const int32_t* px) {
        for (int order = 0; order < kOrders; ++order) {
            const uint64_t h = h_[order];
            uint64_t& e = table_[h >> (64 - bits_)];
            const uint64_t check = (h >> 20) & 0xFF;
            uint64_t vals = 0;
            for (int ch = 0; ch < C_; ++ch) vals |= static_cast<uint64_t>(px[ch] - lo_[ch]) << (13 + 17 * ch);
            int n = 1;
            if ((e & 0xFF) == check && (e & ~((1ull << 13) - 1)) == vals) {
                n = std::min(31, static_cast<int>((e >> 8) & 31) + 1);
            } else if ((e & 0xFF) == check && ((e >> 8) & 31) > 0) {
                alt_[h >> (64 - bits_)] = e;  // keep the replaced value as second candidate
            }
            e = vals | (static_cast<uint64_t>(n) << 8) | check;
        }
    }
    static uint64_t mix(uint64_t h, int32_t v) { return (h ^ static_cast<uint32_t>(v)) * 0x9E3779B97F4A7C15ull; }

private:
    int C_, bits_;
    int lo_[3];
    std::vector<uint64_t> table_, alt_;
    std::vector<BitModel> hit_;
    bool hasAlt_ = false;

public:
    bool hasAlt() const { return hasAlt_; }
    BitModel& altModel(int order, int prevHit) { return altM_[order * 2 + prevHit]; }

private:
    BitModel altM_[kOrders * 2];
    struct Stat {
        uint32_t hits = 0, total = 0;
    };
    Stat stat_[kOrders * 32];
    uint64_t h_[kOrders] = {};
};

// Row input/output for codeSamples without a full image in memory: the
// encoder asks for each row with fill(row, dst) (transformed samples, W * C
// values), the decoder hands over each decoded row with emit(row, src). Only
// the last 3 rows are kept.
struct RowIO {
    std::function<void(int, int32_t*)> fill;
    std::function<void(int, const int32_t*)> emit;
};

template <class Coder>
void codeSamples(Coder& c, Planes& pl, std::vector<double>* chanBits, int r0 = 0, int r1 = -1,
                 const RowIO* io = nullptr) {
    const Tune tp = profileTune(pl.profile);
    const int W = pl.W, C = pl.C;
    int H = pl.H;
    int R[3], half[3], mid[3];
    const int* gshift = pl.gshift;
    std::vector<ResidualModel> models, missModels;
    for (int ch = 0; ch < C; ++ch) {
        R[ch] = pl.hi[ch] - pl.lo[ch] + 1;
        half[ch] = R[ch] / 2;
        mid[ch] = pl.lo[ch] + R[ch] / 2;
        const int nT = tp.tex == 1 ? 256 : tp.tex == 2 ? 729 : tp.tex == 3 ? 2197 : 0;
        models.emplace_back(half[ch], tp.mix != 0, nT, tp.texSign != 0);
        missModels.emplace_back(half[ch], tp.mix != 0, nT, tp.texSign != 0);
    }
    CrossWeight cross[3];
    LMSPair lms[3];
    const bool lmsOn1 = (tp.predMask >> 5) & 1, lmsOn2 = (tp.predMask >> 11) & 1, lmsAny = lmsOn1 || lmsOn2;
    ResidualLMS resLms[3];
    BiasTable bias(static_cast<size_t>(C) * 729);
    std::unique_ptr<MatchModel> match;
    if (tp.match && pl.match) match.reset(new MatchModel(static_cast<size_t>(W) * pl.H, C, pl.lo));
    int prevHit = 0;
    // predictors used for channel 0 and for channels 1-2 (9 and 10 need a
    // reference channel)
    int predLists[2][kNumPred], nPreds[2] = {0, 0};
    for (int k = 0; k < kNumPred; ++k)
        if ((tp.predMask >> k) & 1) {
            if (k != 9 && k != 10) predLists[0][nPreds[0]++] = k;
            predLists[1][nPreds[1]++] = k;
        }
    if (tp.medC && !((tp.predMask >> 4) & 1)) predLists[1][nPreds[1]++] = 4;

    const size_t stride = static_cast<size_t>(W) * C;
    std::vector<int> errPrev(stride, 0), errCur(stride, 0);
    // absolute errors of every predictor (fixed point), previous and current row
    std::vector<uint32_t> perrPrev(stride * kNumPred, 0), perrCur(stride * kNumPred, 0), perrUp3(stride * kNumPred, 0);
    int32_t* d = io ? nullptr : pl.v.data();
    std::vector<int32_t> ring(io ? 3 * stride : 0);  // rows r, r-1, r-2 (streaming)

    // rows r0 .. r1-1 (a stripe, coded independently: r is relative to r0, and
    // nothing above r0 is used)
    if (r1 < 0) r1 = H;
    for (int ry = r0; ry < r1; ++ry) {
        const int r = ry - r0;
        int32_t* cur;
        const int32_t* up;
        const int32_t* up2;
        if (io) {
            cur = &ring[static_cast<size_t>(r % 3) * stride];
            up = r > 0 ? &ring[static_cast<size_t>((r + 2) % 3) * stride] : nullptr;
            up2 = r > 1 ? &ring[static_cast<size_t>((r + 1) % 3) * stride] : up;
            if (Coder::kEncoding) io->fill(ry, cur);
        } else {
            cur = d + static_cast<size_t>(ry) * stride;
            up = r > 0 ? cur - stride : nullptr;
            up2 = r > 1 ? cur - 2 * stride : up;
        }
        for (int x = 0; x < W; ++x) {
            int d0 = 0, e0 = 0;  // channel 0: spatial prediction error, coded residual
            bool hit = false, missed = false;
            int matchOrder = -1, matchCount = 0;
            int32_t matchPred[3] = {0, 0, 0};
            if (match) {
                // contexts of exact neighbouring pixels (absent neighbours: a marker)
                const int32_t kOut = INT32_MIN;
                uint64_t hs = 0x1234567ull, hl;
                auto px = [&](int dx, int dy, int ch) -> int32_t {
                    const int xx = x + dx;
                    if (xx < 0 || xx >= W || r + dy < 0) return kOut;
                    const int32_t* row = dy == 0 ? cur : dy == -1 ? up : up2;
                    return row[static_cast<size_t>(xx) * C + ch];
                };
                for (int ch = 0; ch < C; ++ch) {
                    hs = MatchModel::mix(hs, px(-1, 0, ch));
                    hs = MatchModel::mix(hs, px(0, -1, ch));
                    hs = MatchModel::mix(hs, px(-1, -1, ch));
                    hs = MatchModel::mix(hs, px(1, -1, ch));
                }
                hl = hs;
                for (int ch = 0; ch < C; ++ch) {
                    hl = MatchModel::mix(hl, px(-2, 0, ch));
                    hl = MatchModel::mix(hl, px(0, -2, ch));
                    hl = MatchModel::mix(hl, px(1, -2, ch));
                }
                uint64_t hx = hl;  // longest: a 5x3 window
                for (int ch = 0; ch < C; ++ch) {
                    hx = MatchModel::mix(hx, px(-3, 0, ch));
                    hx = MatchModel::mix(hx, px(-2, -1, ch));
                    hx = MatchModel::mix(hx, px(2, -1, ch));
                    hx = MatchModel::mix(hx, px(-1, -2, ch));
                    hx = MatchModel::mix(hx, px(2, -2, ch));
                }
                const uint64_t h3[3] = {hs, hl, hx};
                int32_t mp[3], ma[3];
                int cnt = 0;
                const int order = match->lookup(h3, mp, cnt, ma);
                matchOrder = order;
                matchCount = cnt;
                for (int ch = 0; ch < C; ++ch) matchPred[ch] = mp[ch];
                if (order >= 0 && match->enabled(order, cnt, tp.matchThr)) {
                    // does the pixel W agree with the prediction's left neighbour pattern?
                    const int same = x > 0 && px(-1, 0, 0) == mp[0];
                    BitModel& bm = match->hitModel(order, cnt, prevHit, same);
                    int bit = 0;
                    if (Coder::kEncoding) {
                        bit = 1;
                        for (int ch = 0; ch < C; ++ch) bit &= cur[static_cast<size_t>(x) * C + ch] == mp[ch];
                    }
                    if (chanBits) c.setStats(&(*chanBits)[0]);
                    hit = codeBit(c, bm, bit) != 0;
                    missed = !hit;
                    if (hit && !Coder::kEncoding)
                        for (int ch = 0; ch < C; ++ch) cur[static_cast<size_t>(x) * C + ch] = mp[ch];
                    if (!hit && tp.matchAlt && match->hasAlt()) {
                        int bit2 = 0;
                        if (Coder::kEncoding) {
                            bit2 = 1;
                            for (int ch = 0; ch < C; ++ch) bit2 &= cur[static_cast<size_t>(x) * C + ch] == ma[ch];
                        }
                        hit = codeBit(c, match->altModel(order, prevHit), bit2) != 0;
                        if (hit && !Coder::kEncoding)
                            for (int ch = 0; ch < C; ++ch) cur[static_cast<size_t>(x) * C + ch] = ma[ch];
                    }
                }
                prevHit = hit;
            }
            for (int ch = 0; ch < C; ++ch) {
                const size_t i = static_cast<size_t>(x) * C + ch;
                int Wv, Nv, NWv, NEv, WWv, NNv, NNEv;
                if (r == 0) {
                    Wv = x > 0 ? cur[i - C] : mid[ch];
                    Nv = NWv = NEv = NNv = NNEv = Wv;
                    WWv = x > 1 ? cur[i - 2 * C] : Wv;
                } else {
                    Nv = up[i];
                    Wv = x > 0 ? cur[i - C] : Nv;
                    NWv = x > 0 ? up[i - C] : Nv;
                    NEv = x + 1 < W ? up[i + C] : Nv;
                    WWv = x > 1 ? cur[i - 2 * C] : Wv;
                    NNv = up2[i];
                    NNEv = x + 1 < W ? up2[i + C] : NNv;
                }
                Neighbours n{Wv, Nv, NWv, NEv};

                // candidate predictions (fixed point)
                int pk[kNumPred];
                pk[0] = Wv << kFP;
                pk[1] = Nv << kFP;
                pk[2] = (Wv + NEv) << (kFP - 1);
                pk[3] = (Wv + Nv - NWv) << kFP;
                pk[4] = predictMED(n) << kFP;
                const int NEEv = r > 0 && x + 2 < W ? up[i + 2 * C] : NEv;
                const int NWWv = r > 0 && x > 1 ? up[i - 2 * C] : NWv;
                const int NNWv = r > 1 && x > 0 ? up2[i - C] : NNv;
                int32_t lu[LMSPair::kTaps] = {Wv - Nv,   NWv - Nv,  NEv - Nv,  WWv - Nv, NNv - Nv, NNEv - Nv,
                                          NEEv - Nv, NWWv - Nv, NNWv - Nv, 0,        0,        0};
                for (int rc = 0; rc < ch && rc < 2; ++rc) {  // reference channels
                    const size_t j = static_cast<size_t>(x) * C + (ch - 1 - rc);
                    const int X0 = cur[j];
                    const int N0 = r > 0 ? up[j] : (x > 0 ? cur[j - C] : X0);
                    const int W0 = x > 0 ? cur[j - C] : N0;
                    if (rc == 0) {
                        lu[9] = X0 - N0;
                        lu[10] = W0 - N0;
                    } else {
                        lu[11] = X0 - N0;
                    }
                }
                {
                    int64_t lp1 = 0, lp2 = 0;
                    if (lmsAny) lms[ch].predict(lu, lp1, lp2);
                    lp1 += static_cast<int64_t>(Nv) << kFP;
                    lp2 += static_cast<int64_t>(Nv) << kFP;
                    const int64_t lo8 = static_cast<int64_t>(pl.lo[ch]) << kFP, hi8 = static_cast<int64_t>(pl.hi[ch]) << kFP;
                    pk[5] = static_cast<int>(lp1 < lo8 ? lo8 : (lp1 > hi8 ? hi8 : lp1));
                    pk[11] = static_cast<int>(lp2 < lo8 ? lo8 : (lp2 > hi8 ? hi8 : lp2));
                }
                pk[6] = (Wv + NEv - Nv) << kFP;
                pk[7] = (2 * Wv - WWv) << kFP;
                pk[8] = (2 * Nv - NNv) << kFP;
                const int* predList = predLists[ch > 0];
                const int nPredHere = nPreds[ch > 0];
                if (ch > 0) {
                    // cross-channel: same local gradient as in the reference channel
                    const int rc = tp.crossRef ? ch - 1 : 0;
                    const size_t j = static_cast<size_t>(x) * C + rc;
                    const int X0 = cur[j];
                    const int W0 = x > 0 ? cur[j - C] : (r > 0 ? up[j] : X0);
                    const int N0 = r > 0 ? up[j] : W0;
                    pk[9] = (Wv + X0 - W0) << kFP;
                    pk[10] = (Nv + X0 - N0) << kFP;
                }

                // blend: weights from the recent errors of each predictor
                const uint32_t* eN = r > 0 ? &perrPrev[i * kNumPred] : nullptr;
                const uint32_t* eW = x > 0 ? &perrCur[(i - C) * kNumPred] : nullptr;
                const uint32_t* eNW = r > 0 && x > 0 ? &perrPrev[(i - C) * kNumPred] : nullptr;
                const uint32_t* eNE = r > 0 && x + 1 < W ? &perrPrev[(i + C) * kNumPred] : nullptr;
                int64_t sw = 0, swp = 0;
                uint64_t minErr = ~0ull;
                int pmin = INT32_MAX, pmax = INT32_MIN;
                const uint32_t* e3 = r > 0 ? &perrUp3[i * kNumPred] : nullptr;  // NW + N + NE
                const bool fast3 = tp.errNW == 2;
                for (int j = 0; j < nPredHere; ++j) {
                    const int k = predList[j];
                    uint64_t es = 1;
                    if (eW) es += 2ull * eW[k];
                    if (fast3) {
                        if (e3) es += 2ull * e3[k];
                    } else {
                        if (eN) es += 2ull * eN[k];
                        if (eNW) es += static_cast<uint64_t>(tp.errNW) * eNW[k];
                        if (eNE) es += static_cast<uint64_t>(tp.errNW) * eNE[k];
                    }
                    minErr = std::min(minErr, es);
                    pmin = std::min(pmin, pk[k]);
                    pmax = std::max(pmax, pk[k]);
                    const int64_t w = tp.blendPow >= 2 ? blendWeight(es * es) : blendWeight(es);
                    sw += w;
                    swp += w * pk[k];
                }
                int pred8 = sw > 0 ? static_cast<int>((swp + sw / 2) / sw) : pk[4];

                // bias correction
                const size_t bctx = static_cast<size_t>(ch) * 729 +
                                    static_cast<size_t>((quantGrad(NEv - Nv, gshift[ch]) + 4) * 81 +
                                                        (quantGrad(Nv - NWv, gshift[ch]) + 4) * 9 +
                                                        (quantGrad(NWv - Wv, gshift[ch]) + 4));
                const int predNoBias = pred8;
                if (tp.bias) pred8 += bias.get(bctx);
                // second stage: NLMS on the residuals of the neighbours
                int32_t ru[ResidualLMS::kTaps] = {0};
                int64_t casc = 0;
                const bool doCasc = tp.cascade == 1 || (tp.cascade == 2 && ch == 0);
                if (doCasc) {
                    ru[0] = x > 0 ? errCur[i - C] : 0;
                    ru[1] = errPrev[i];
                    ru[2] = x > 0 ? errPrev[i - C] : 0;
                    ru[3] = x + 1 < W ? errPrev[i + C] : 0;
                    ru[4] = x > 1 ? errCur[i - 2 * C] : 0;
                    ru[5] = ch > 0 && tp.cascE0 ? e0 : 0;
                    casc = resLms[ch].predict(ru);
                    pred8 += static_cast<int>(casc);
                }

                int predS = (pred8 + (1 << (kFP - 1))) >> kFP;
                if (predS < pl.lo[ch]) predS = pl.lo[ch];
                if (predS > pl.hi[ch]) predS = pl.hi[ch];
                int pred = predS;
                if (ch > 0 && tp.icp) pred += (cross[ch].weight() * d0) >> 8;
                if (pred < pl.lo[ch]) pred = pl.lo[ch];
                if (pred > pl.hi[ch]) pred = pl.hi[ch];

                const int reW = x > 0 ? errCur[i - C] : 0;
                const int reN = errPrev[i];
                uint32_t metric = 0;
                if (tp.metric != 2) metric = static_cast<uint32_t>(activity(n) + tp.wErr * (std::abs(reW) + std::abs(reN)));
                if (tp.metric >= 1) metric += static_cast<uint32_t>(std::min<uint64_t>(tp.wBlend * (minErr >> kFP), 1u << 30));
                if (ch > 0) metric += static_cast<uint32_t>(tp.wCross * std::abs(e0));
                if (tp.wSpreadQ) metric += static_cast<uint32_t>(((pmax - pmin) >> kFP) * tp.wSpreadQ) >> 1;
                metric >>= tp.ctxShift;
                int q;
                if (tp.fineQ) {  // two levels per octave
                    const int lm = bitLength(metric);
                    q = lm < 2 ? static_cast<int>(metric) : 2 * lm - 2 + static_cast<int>((metric >> (lm - 2)) & 1);
                } else {
                    q = bitLength(metric);
                }
                q = std::min(q, ResidualModel::kActivityCtx - 1);
                int zctx = (reW == 0) + (reN == 0);
                // sign context: signs of the neighbouring residuals and, when the
                // inter-channel correction is not applied, where the exact (fixed-point)
                // prediction falls relative to the rounded one
                const int frac = pred8 - (predS << kFP);
                const int fb = (pred != predS || !tp.signFrac) ? 1 : (frac < 0 ? 0 : (frac > 0 ? 2 : 1));
                const int sctx = 9 * fb + 3 * ((reW > 0) - (reW < 0) + 1) + ((reN > 0) - (reN < 0) + 1);
                if (tp.zeroFrac) {  // a prediction halfway between two values is rarely exact
                    const int af = pred != predS ? 0 : std::abs(frac);
                    zctx = zctx * 3 + (af <= 1 ? 0 : af <= 2 ? 1 : 2);
                }

                int e = 0;
                if (Coder::kEncoding || hit) {
                    e = cur[i] - pred;  // modular reduction to [-half, R-half-1]
                    if (e < -half[ch]) e += R[ch];
                    else if (e >= R[ch] - half[ch]) e -= R[ch];
                }
                if (chanBits) c.setStats(&(*chanBits)[ch]);
                // second context: expected error of the blend (and, for channels 1-2,
                // the residual of channel 0), quantised on a finer log scale
                uint32_t mb = static_cast<uint32_t>(std::min<uint64_t>(minErr >> (kFP - 1), 1u << 24));
                if (tp.qbMode == 1) mb += static_cast<uint32_t>(std::abs(reW) + std::abs(reN)) * 2;
                if (ch > 0 && tp.qbMode != 2) mb += static_cast<uint32_t>(std::abs(e0)) * 2;
                if (tp.wSpread) mb += static_cast<uint32_t>(((pmax - pmin) >> kFP) * tp.wSpread) >> 1;
                const int lb = bitLength(mb);
                const int qbv = lb < 2 ? static_cast<int>(mb) : 2 * lb - 2 + static_cast<int>((mb >> (lb - 2)) & 1);
                const int qb = std::min(qbv, ResidualModel::kCtxB - 1);
                int tctx = 0;
                if (tp.tex == 2) {
                    tctx = static_cast<int>(bctx % 729);
                } else if (tp.tex == 3) {
                    auto fq = [&](int dd) {
                        const int a = (dd < 0 ? -dd : dd) >> gshift[ch];
                        const int qq = a <= 2 ? a : a <= 4 ? 3 : a <= 8 ? 4 : a <= 20 ? 5 : 6;
                        return dd < 0 ? -qq : qq;
                    };
                    tctx = (fq(NEv - Nv) + 6) * 169 + (fq(Nv - NWv) + 6) * 13 + (fq(NWv - Wv) + 6);
                } else if (tp.tex) {
                    const int eqp = (Wv == Nv) | (Wv == NWv) << 1 | (Nv == NWv) << 2 | (Nv == NEv) << 3 |
                                    (WWv == Wv) << 4 | (NNv == Nv) << 5;
                    tctx = eqp * 4 + std::min(qb >> 3, 3);
                }
                // after a failed match the pixel is "surprising": separate models
                if (!hit) e = (missed && tp.missModels ? missModels[ch] : models[ch]).code(c, e, q, zctx, sctx, qb, tctx);
                if (!Coder::kEncoding && !hit) {
                    int v = pred + e;
                    if (v < pl.lo[ch]) v += R[ch];
                    else if (v > pl.hi[ch]) v -= R[ch];
                    cur[i] = v;
                }
                errCur[i] = e;

                // update the statistics with the true value
                const int x8 = cur[i] << kFP;
                uint32_t* pe = &perrCur[i * kNumPred];
                for (int j = 0; j < nPredHere; ++j) {
                    const int k = predList[j];
                    pe[k] = static_cast<uint32_t>(std::abs(x8 - pk[k]));
                }
                if (tp.bias) bias.update(bctx, x8 - predNoBias, tp.biasLimit);
                if (doCasc) resLms[ch].update(ru, static_cast<int64_t>(x8) - pred8, ResidualLMS::norm(ru), tp.cascMu);
                if (lmsAny)
                    lms[ch].update(lu, static_cast<int64_t>(x8) - pk[5], static_cast<int64_t>(x8) - pk[11], tp.lmsMu,
                                   tp.lmsMu2, lmsOn1, lmsOn2);
                const int dS = cur[i] - predS;  // spatial prediction error
                if (ch == 0) {
                    d0 = dS;
                    e0 = e;
                } else if (tp.icp) {
                    cross[ch].update(d0, dS, tp.icpDecay);
                }
            }
            if (match) {
                const int32_t* pxv = &cur[static_cast<size_t>(x) * C];
                if (matchOrder >= 0) {
                    bool wh = true;
                    for (int ch = 0; ch < C; ++ch) wh = wh && pxv[ch] == matchPred[ch];
                    match->record(matchOrder, matchCount, wh);
                }
                match->update(pxv);
            }
        }
        if (io && !Coder::kEncoding) io->emit(ry, cur);
        std::swap(errPrev, errCur);
        // sums of the errors at NW, N and NE for the next row
        for (int x = 0; x < W; ++x)
            for (int ch = 0; ch < C; ++ch) {
                const size_t i = static_cast<size_t>(x) * C + ch;
                for (int j = 0; j < nPreds[ch > 0]; ++j) {
                    const int k = predLists[ch > 0][j];
                    uint32_t v = perrCur[i * kNumPred + k];
                    if (x > 0) v += perrCur[(i - C) * kNumPred + k];
                    if (x + 1 < W) v += perrCur[(i + C) * kNumPred + k];
                    perrUp3[i * kNumPred + k] = v;
                }
            }
        std::swap(perrPrev, perrCur);
    }
    c.setStats(nullptr);
}

// Runs f(0) .. f(n-1) on up to `threads` threads; rethrows the first exception.
template <class F>
void runParallel(int n, int threads, F f) {
    threads = std::max(1, std::min(threads, n));
    if (threads == 1) {
        for (int k = 0; k < n; ++k) f(k);
        return;
    }
    std::atomic<int> next(0);
    std::exception_ptr err;
    std::mutex m;
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t)
        pool.emplace_back([&]() {
            for (int k; (k = next++) < n;) {
                try {
                    f(k);
                } catch (...) {
                    std::lock_guard<std::mutex> g(m);
                    if (!err) err = std::current_exception();
                }
            }
        });
    for (auto& th : pool) th.join();
    if (err) std::rethrow_exception(err);
}

inline int stripeStart(int H, int S, int k) { return static_cast<int>(static_cast<int64_t>(H) * k / S); }

// Rows used to compare coding options cheaply: bands of 16 rows spread over
// the image, about 64K pixels in total (all rows for small images).
std::vector<int> sampleRows(int W, int H) {
    const int want = std::max(32, 65536 / std::max(W, 1));
    std::vector<int> rows;
    if (H <= want) {
        for (int r = 0; r < H; ++r) rows.push_back(r);
        return rows;
    }
    const int bands = (want + 15) / 16;
    for (int b = 0; b < bands; ++b) {
        const int start = static_cast<int>((static_cast<int64_t>(H - 16) * (2 * b + 1)) / (2 * bands));
        for (int r = start; r < start + 16; ++r) rows.push_back(r);
    }
    return rows;
}

// Colour transforms tried by the encoder (bit t = transform t).
const int kTransformCandidates = std::getenv("IC_TCAND") ? std::atoi(std::getenv("IC_TCAND")) : 0x7;

// Samples of a binary image, read directly from the bytes of the file (the
// image is never copied as a whole).
struct Source {
    const uint8_t* p = nullptr;
    int W = 0, H = 0, C = 1, bps = 1;
    bool be = true;
    size_t n() const { return static_cast<size_t>(W) * H * C; }
    uint16_t at(size_t i) const {
        if (bps == 1) return p[i];
        const uint8_t* q = p + 2 * i;
        return static_cast<uint16_t>(be ? (q[0] << 8) | q[1] : q[0] | (q[1] << 8));
    }
    void row(int r, uint16_t* out) const {
        const size_t base = static_cast<size_t>(r) * W * C, m = static_cast<size_t>(W) * C;
        for (size_t k = 0; k < m; ++k) out[k] = at(base + k);
    }
};

// Builds row r of the planes that are coded: value scaling (v -> (v - off) /
// step, per channel), colour transform t and, for packed grey images, the rank
// of each value.
struct RowMaker {
    const Source* src = nullptr;
    int t = kNone;
    const std::vector<int32_t>* rank = nullptr;  // packed: value - packMin -> rank
    int packMin = 0;
    int step[3] = {1, 1, 1}, off[3] = {0, 0, 0};
    bool scaled = false;
    void operator()(int r, int32_t* out) const {
        thread_local std::vector<uint16_t> tmp;
        tmp.resize(static_cast<size_t>(src->W) * src->C);
        src->row(r, tmp.data());
        if (scaled) {
            const size_t m = static_cast<size_t>(src->W) * src->C;
            for (size_t k = 0; k < m; ++k) {
                const int ch = static_cast<int>(k % src->C);
                tmp[k] = static_cast<uint16_t>((tmp[k] - off[ch]) / step[ch]);
            }
        }
        forwardTransform(t, src->C, tmp.data(), out, static_cast<size_t>(src->W));
        if (rank)
            for (int x = 0; x < src->W; ++x) out[x] = (*rank)[static_cast<size_t>(out[x] - packMin)];
    }
};

// Gradient scale of each channel: the bias contexts quantise gradients with
// thresholds made for 8-bit data, so they are divided by 2^gshift, where the
// spread of the central 99.8% of the values has 8 + gshift bits. (Using the
// whole range would be misled by a few outliers, e.g. saturated pixels.)
// Computed row by row from the coded planes.
void computeGradShiftRows(Planes& pl, const RowMaker& mk) {
    const size_t m = static_cast<size_t>(pl.W) * pl.C;
    std::vector<int32_t> row(m);
    std::vector<std::vector<uint32_t>> hist(static_cast<size_t>(pl.C));
    for (int ch = 0; ch < pl.C; ++ch) hist[ch].assign(static_cast<size_t>(pl.hi[ch] - pl.lo[ch] + 1), 0);
    for (int r = 0; r < pl.H; ++r) {
        mk(r, row.data());
        for (size_t k = 0; k < m; ++k) {
            const int ch = static_cast<int>(k % pl.C);
            ++hist[ch][static_cast<size_t>(row[k] - pl.lo[ch])];
        }
    }
    const size_t n = static_cast<size_t>(pl.W) * pl.H;
    for (int ch = 0; ch < pl.C; ++ch) {
        const int R = pl.hi[ch] - pl.lo[ch] + 1;
        const auto& h = hist[ch];
        const uint64_t cut = n / 1000;
        uint64_t acc = 0;
        int a = 0, b = R - 1;
        while (a < R - 1 && acc + h[a] <= cut) acc += h[a++];
        acc = 0;
        while (b > a && acc + h[b] <= cut) acc += h[b--];
        pl.gshift[ch] = std::max(0, bitLength(static_cast<uint32_t>(b - a)) - 8);
    }
}

// The given rows of the coded planes, as a small Planes in memory (for the
// dry runs that compare coding options).
void sampleRowsPlanes(const Planes& info, const RowMaker& mk, const std::vector<int>& rows, Planes& sp) {
    sp = info;
    sp.H = static_cast<int>(rows.size());
    const size_t stride = static_cast<size_t>(info.W) * info.C;
    sp.v.assign(rows.size() * stride, 0);
    for (size_t k = 0; k < rows.size(); ++k) mk(rows[k], &sp.v[k * stride]);
}

// Distinct colours of the image (packed as R<<32 | G<<16 | B, or the grey
// level), sorted by luminance, read from the file. Returns false if there are
// more than maxK.
bool findPaletteSrc(const Source& src, std::vector<uint64_t>& pal, size_t maxK) {
    const int C = src.C;
    std::vector<uint64_t> sorted;
    uint64_t last = ~0ull;
    const size_t np = static_cast<size_t>(src.W) * src.H;
    for (size_t p = 0; p < np; ++p) {
        uint64_t key = 0;
        for (int ch = 0; ch < C; ++ch) key = (key << 16) | src.at(p * C + ch);
        if (key == last) continue;
        last = key;
        auto it = std::lower_bound(sorted.begin(), sorted.end(), key);
        if (it == sorted.end() || *it != key) {
            if (sorted.size() >= maxK) return false;
            sorted.insert(it, key);
        }
    }
    auto luma = [C](uint64_t k) {
        if (C == 1) return static_cast<uint64_t>(k);
        const uint64_t R = (k >> 32) & 0xFFFF, G = (k >> 16) & 0xFFFF, B = k & 0xFFFF;
        return (299 * R + 587 * G + 114 * B) * 65536 + (k & 0xFFFF);
    };
    pal = sorted;
    std::stable_sort(pal.begin(), pal.end(), [&](uint64_t a, uint64_t b) { return luma(a) < luma(b); });
    return true;
}

// Palette index of every pixel of a row (from the file).
struct IndexMaker {
    const Source* src = nullptr;
    std::vector<uint64_t> sorted;  // colours sorted by key
    std::vector<int32_t> rankOf;   // position in `sorted` -> palette index
    void init(const Source* s, const std::vector<uint64_t>& pal) {
        src = s;
        sorted = pal;
        std::sort(sorted.begin(), sorted.end());
        rankOf.assign(pal.size(), 0);
        for (size_t k = 0; k < pal.size(); ++k)
            rankOf[std::lower_bound(sorted.begin(), sorted.end(), pal[k]) - sorted.begin()] = static_cast<int32_t>(k);
    }
    void operator()(int r, int32_t* out) const {
        const int C = src->C;
        for (int x = 0; x < src->W; ++x) {
            uint64_t key = 0;
            const size_t base = (static_cast<size_t>(r) * src->W + x) * C;
            for (int ch = 0; ch < C; ++ch) key = (key << 16) | src->at(base + ch);
            out[x] = rankOf[std::lower_bound(sorted.begin(), sorted.end(), key) - sorted.begin()];
        }
    }
};

std::vector<uint8_t> encodeBytesMode(const std::vector<uint8_t>& file, uint32_t crc, CodecStats* st,
                                     const std::string& why) {
    std::vector<uint8_t> out = {'I', 'C', 'L', 'L', kVersion, kModeBytes};
    put(out, file.size(), 8);
    put(out, crc, 4);
    RangeEncoder enc(out);
    double bits = 0;
    if (st) enc.setStats(&bits);
    std::vector<uint8_t> tmp(file);
    codeBytes(enc, tmp.data(), tmp.size());
    enc.finish();
    if (st) {
        st->mode = "bytes (" + why + ")";
        st->otherBits = bits;
        st->originalBytes = file.size();
        st->compressedBytes = out.size();
    }
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------

static std::vector<uint8_t> encodeModel(const std::vector<uint8_t>& file, uint32_t crc, const RawParams& raw,
                                        CodecStats* st, const EncodeOptions& opt);

std::vector<uint8_t> losslessEncode(const std::vector<uint8_t>& file, const RawParams& raw, CodecStats* st,
                                    const EncodeOptions& opt) {
    const uint32_t crc = crc32(file.data(), file.size());
    std::vector<uint8_t> out = encodeModel(file, crc, raw, st, opt);
    // Incompressible data (noise, tiny files): store it as it is.
    if (out.size() > file.size() + 18) {
        out = {'I', 'C', 'L', 'L', kVersion, kModeStored};
        put(out, file.size(), 8);
        put(out, crc, 4);
        out.insert(out.end(), file.begin(), file.end());
        if (st) {
            st->mode = "stored (not compressible)";
            st->channelBits.clear();
            st->pixels = 0;
            st->compressedBytes = out.size();
        }
    }
    return out;
}

static std::vector<uint8_t> encodeModel(const std::vector<uint8_t>& file, uint32_t crc, const RawParams& raw,
                                        CodecStats* st, const EncodeOptions& opt) {
    // Only the header is parsed: the samples are read from the file bytes when
    // needed, so the image is never held in memory as a whole (only the file).
    Image img;
    try {
        img = parseImageHeader(file, raw);
    } catch (const std::exception& e) {
        if (raw.given()) throw;  // wrong RAW parameters: report it
        return encodeBytesMode(file, crc, st, std::string("not a PGM/PPM: ") + e.what());
    }
    if (img.ascii) return encodeBytesMode(file, crc, st, "ASCII PNM");
    Source src;
    src.p = file.data() + img.rasterOffset;
    src.W = img.width;
    src.H = img.height;
    src.C = img.channels;
    src.bps = img.bpp / 8;
    src.be = img.bigEndian;
    // Value scaling: if all the values of a channel are off + k * step (e.g.
    // 12-bit data stored in the top bits of 16-bit samples), code k instead.
    int step[3] = {1, 1, 1}, off[3] = {0, 0, 0};
    {
        int first[3] = {-1, -1, -1}, g[3] = {0, 0, 0};
        for (size_t i = 0; i < src.n(); ++i) {
            const int v = src.at(i), ch = static_cast<int>(i % src.C);
            if (static_cast<uint32_t>(v) > img.maxval) return encodeBytesMode(file, crc, st, "samples above maxval");
            if (first[ch] < 0) first[ch] = v;
            else if (g[ch] != 1) g[ch] = std::gcd(g[ch], std::abs(v - first[ch]));
        }
        for (int ch = 0; ch < src.C; ++ch)
            if (g[ch] > 1) {
                step[ch] = g[ch];
                off[ch] = first[ch] % g[ch];
            }
    }
    const bool scaled = step[0] > 1 || step[1] > 1 || step[2] > 1;
    uint32_t codedMax = img.maxval;  // largest value after the scaling
    if (scaled) {
        codedMax = 0;
        for (int ch = 0; ch < src.C; ++ch)
            codedMax = std::max(codedMax, (img.maxval - static_cast<uint32_t>(off[ch])) / static_cast<uint32_t>(step[ch]));
    }
    // The samples are coded exactly as they are stored (1 or 2 bytes, same byte
    // order), so header + samples + trailer rebuild the file byte for byte.
    const size_t hdr = img.rasterOffset, end = hdr + src.n() * src.bps;
    if (end > file.size()) return encodeBytesMode(file, crc, st, "truncated raster");

    std::vector<uint8_t> out = {'I', 'C', 'L', 'L', kVersion, kModeImage};
    put(out, file.size(), 8);
    put(out, crc, 4);
    put(out, static_cast<uint8_t>(img.format), 1);
    put(out, static_cast<uint32_t>(img.channels), 1);
    put(out, static_cast<uint32_t>(img.bpp), 1);
    put(out, img.bigEndian ? 1 : 0, 1);
    put(out, static_cast<uint32_t>(img.width), 4);
    put(out, static_cast<uint32_t>(img.height), 4);
    put(out, img.maxval, 4);
    put(out, hdr, 4);
    out.insert(out.end(), file.begin(), file.begin() + static_cast<long>(hdr));
    const size_t trailer = file.size() - end;
    put(out, trailer, 8);

    // Colour transform: the candidate that codes a sample of the rows with the
    // fewest bits (dry run of the real coder). IC_TRANSFORM forces one.
    Planes pl;  // channel ranges and parameters (no samples: rows are made on demand)
    pl.W = img.width;
    pl.H = img.height;
    pl.C = img.channels;
    pl.profile = opt.profile;
    pl.match = 0;  // decided below
    RowMaker mk;
    mk.src = &src;
    mk.scaled = scaled;
    for (int ch = 0; ch < 3; ++ch) {
        mk.step[ch] = step[ch];
        mk.off[ch] = off[ch];
    }
    const std::vector<int> rows = sampleRows(img.width, img.height);
    int best = kNone;
    if (img.channels == 3) {
        const char* force = std::getenv("IC_TRANSFORM");
        double bestCost = 0;
        Planes sp;
        for (int t = 0; t < kNumTransforms; ++t) {
            if (force && std::atoi(force) != t) continue;
            if (!force && !((kTransformCandidates >> t) & 1)) continue;
            if (force) {
                best = t;
                break;
            }
            mk.t = t;
            transformRange(t, 3, static_cast<int>(codedMax), pl.lo, pl.hi);
            computeGradShiftRows(pl, mk);
            sampleRowsPlanes(pl, mk, rows, sp);
            CostCoder cc;
            codeSamples(cc, sp, nullptr);
            if (t == 0 || cc.cost < bestCost) {
                bestCost = cc.cost;
                best = t;
            }
        }
    }
    mk.t = best;
    transformRange(best, img.channels, static_cast<int>(codedMax), pl.lo, pl.hi);
    computeGradShiftRows(pl, mk);

    // Value packing (grey images): if only some of the values between the
    // minimum and the maximum occur (e.g. data scaled non-linearly), code the
    // rank of each value among the values used, when that is cheaper on the
    // sample. The set of values used is sent as a bitmap.
    bool usePack = false;
    int packMin = 0;
    std::vector<uint8_t> packMap;
    std::vector<int32_t> packRank;
    if (img.channels == 1 && (!std::getenv("IC_PACK") || std::atoi(std::getenv("IC_PACK")) != 0)) {
        std::vector<uint32_t> hist(static_cast<size_t>(codedMax) + 1, 0);
        for (size_t i = 0; i < src.n(); ++i) ++hist[static_cast<size_t>((src.at(i) - off[0]) / step[0])];
        int mn = 0, mx = static_cast<int>(codedMax);
        while (mn < mx && !hist[mn]) ++mn;
        while (mx > mn && !hist[mx]) --mx;
        const int span = mx - mn + 1;
        std::vector<int32_t> rank(static_cast<size_t>(span), 0);
        int K = 0;
        for (int v = mn; v <= mx; ++v) rank[v - mn] = hist[v] ? K++ : K;
        if (K < span && K > 1) {
            Planes pp = pl;
            RowMaker mkp = mk;
            mkp.rank = &rank;
            mkp.packMin = mn;
            pp.lo[0] = 0;
            pp.hi[0] = K - 1;
            computeGradShiftRows(pp, mkp);
            std::vector<uint8_t> map(static_cast<size_t>(span + 7) / 8, 0);
            for (int v = mn; v <= mx; ++v)
                if (hist[v]) map[(v - mn) >> 3] |= static_cast<uint8_t>(1 << ((v - mn) & 7));
            Planes s1, s2;
            sampleRowsPlanes(pl, mk, rows, s1);
            sampleRowsPlanes(pp, mkp, rows, s2);
            CostCoder c1, c2, cm;
            codeSamples(c1, s1, nullptr);
            codeSamples(c2, s2, nullptr);
            std::vector<uint8_t> tmp(map);
            codeBytes(cm, tmp.data(), tmp.size());
            const double scale = static_cast<double>(img.height) / rows.size();
            usePack = (c2.cost * scale + cm.cost < c1.cost * scale * 1.003) || (std::getenv("IC_PACK") && std::atoi(std::getenv("IC_PACK")) == 2);
            if (usePack) {
                pl = pp;
                packMin = mn;
                packMap = map;
                packRank = std::move(rank);
                mk.rank = &packRank;
                mk.packMin = mn;
            }
        }
    }

    // Match model: only kept if it saves at least 1% on the sample (it costs
    // time, and photographs rarely repeat exactly). The decisions above were
    // taken without it (faster; they hardly depend on it).
    double predCost = 0;  // cost of the sample in the predictive mode, as chosen
    {
        Planes sp;
        sampleRowsPlanes(pl, mk, rows, sp);
        CostCoder c0, c1;
        sp.match = 0;
        codeSamples(c0, sp, nullptr);
        predCost = c0.cost;
        pl.match = 0;
        if (tune().match) {
            sp.match = 1;
            codeSamples(c1, sp, nullptr);
            if (c1.cost < c0.cost * (1.0 - tune().matchGain / 1000.0)) {
                pl.match = 1;
                predCost = c1.cost;
            }
        }
    }

    // Palette mode, if the image has few colours and it is cheaper on a sample
    // of the rows than the predictive mode (IC_PALETTE=0/1 forces the choice).
    std::vector<uint64_t> pal;
    bool usePalette = false;
    IndexMaker im;
    const char* forcePal = std::getenv("IC_PALETTE");
    if ((!forcePal || std::atoi(forcePal) != 0) && findPaletteSrc(src, pal, 256)) {
        im.init(&src, pal);
        usePalette = forcePal && std::atoi(forcePal) == 1;
        if (!usePalette) {
            CostCoder cc2;
            std::vector<int32_t> idx(rows.size() * static_cast<size_t>(img.width));
            for (size_t k = 0; k < rows.size(); ++k) im(rows[k], &idx[k * img.width]);
            codeIndices(cc2, idx, img.width, static_cast<int>(pal.size()), 0, static_cast<int>(rows.size()));
            const double palBits = 16.0 * img.channels * pal.size() * rows.size() / img.height;
            // compared with the predictive mode as chosen above (packed or not, match or not)
            usePalette = cc2.cost + palBits < predCost;
        }
    }
    if (usePalette) usePack = false;
    put(out, static_cast<uint32_t>(best), 1);
    put(out, usePalette ? pal.size() : 0, 2);
    for (int ch = 0; ch < 3; ++ch) put(out, static_cast<uint32_t>(ch < pl.C ? pl.gshift[ch] : 0), 1);
    put(out, scaled && !usePalette ? 1 : 0, 1);
    if (scaled && !usePalette)
        for (int ch = 0; ch < 3; ++ch) {
            put(out, static_cast<uint32_t>(step[ch]), 2);
            put(out, static_cast<uint32_t>(off[ch]), 2);
        }
    put(out, usePack ? 1 : 0, 1);
    if (usePack) {
        put(out, static_cast<uint32_t>(packMin), 4);
        put(out, static_cast<uint32_t>(packMap.size()), 4);
    }
    put(out, static_cast<uint32_t>(pl.profile), 1);
    put(out, static_cast<uint32_t>(pl.match), 1);

    // Streams: one with the side information (palette entries or bitmap of the
    // packed values, then the trailer bytes) and one per stripe of rows. With
    // more than one stripe, the stripes are coded independently, in parallel.
    const int S = std::max(1, std::min(opt.threads, std::min(img.height, 4096)));
    put(out, static_cast<uint32_t>(S), 2);
    std::vector<uint8_t> side;
    std::vector<std::vector<uint8_t>> streams(static_cast<size_t>(S));
    std::vector<std::vector<double>> sbits(static_cast<size_t>(S), std::vector<double>(img.channels, 0.0));
    std::vector<double> chanBits(img.channels, 0.0);
    double sideBits = 0, other = 0;
    {
        RangeEncoder enc(side);
        if (st) enc.setStats(&sideBits);
        if (usePalette) {
            std::vector<uint8_t> pb;
            for (uint64_t k : pal)
                for (int ch = img.channels - 1; ch >= 0; --ch) put(pb, (k >> (16 * ch)) & 0xFFFF, 2);
            codeBytes(enc, pb.data(), pb.size());
        } else if (usePack) {
            codeBytes(enc, packMap.data(), packMap.size());
        }
        if (st) enc.setStats(&other);
        if (trailer) {
            std::vector<uint8_t> t(file.begin() + static_cast<long>(end), file.end());
            codeBytes(enc, t.data(), t.size());
        }
        enc.setStats(nullptr);
        enc.finish();
    }
    runParallel(S, opt.threads, [&](int k) {
        RangeEncoder enc(streams[static_cast<size_t>(k)]);
        const int r0 = stripeStart(img.height, S, k), r1 = stripeStart(img.height, S, k + 1);
        if (usePalette) {
            if (st) enc.setStats(&sbits[static_cast<size_t>(k)][0]);
            codeIndexRows(enc, img.width, static_cast<int>(pal.size()), r0, r1, im, [](int, const int32_t*) {});
            enc.setStats(nullptr);
        } else {
            RowIO io;
            io.fill = mk;
            codeSamples(enc, pl, st ? &sbits[static_cast<size_t>(k)] : nullptr, r0, r1, &io);
        }
        enc.finish();
    });
    for (const auto& b : sbits)
        for (size_t ch = 0; ch < b.size(); ++ch) chanBits[ch] += b[ch];
    if (usePalette) chanBits.assign(1, chanBits[0] + sideBits);
    put(out, side.size(), 4);
    for (const auto& str : streams) put(out, str.size(), 4);
    out.insert(out.end(), side.begin(), side.end());
    for (const auto& str : streams) out.insert(out.end(), str.begin(), str.end());

    if (st) {
        st->mode = std::string(pl.profile == 1 ? "[fast] " : pl.profile == 2 ? "[fastest] " : "") +
                   (S > 1 ? "[" + std::to_string(S) + " stripes] " : std::string()) +
                   (usePalette            ? "image, palette of " + std::to_string(pal.size()) + " colours"
                    : usePack             ? std::string("image, predictive, packed values")
                    : img.channels == 3 ? std::string("image, predictive, colour transform ") + transformName(best)
                                        : std::string("image, predictive"));
        if (!usePalette && pl.match) st->mode += ", match model";
        if (!usePalette && scaled) {
            st->mode += ", values scaled (step";
            for (int ch = 0; ch < img.channels; ++ch) st->mode += " " + std::to_string(step[ch]);
            st->mode += ")";
        }
        st->description = img.describe();
        st->channelBits = chanBits;
        st->otherBits = other;
        st->pixels = img.numPixels();
        st->originalBytes = file.size();
        st->compressedBytes = out.size();
    }
    return out;
}

std::vector<uint8_t> losslessDecode(const std::vector<uint8_t>& comp, CodecStats* st, int threads) {
    if (threads <= 0) threads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
    if (comp.size() < 6 || std::memcmp(comp.data(), "ICLL", 4) != 0)
        throw std::runtime_error("not a file produced by img_lossless_enc");
    if (comp[4] != kVersion) throw std::runtime_error("unsupported version " + std::to_string(comp[4]));
    const uint8_t mode = comp[5];
    Reader rd{comp, 6};
    const uint64_t size = rd.get(8);
    const uint32_t crc = static_cast<uint32_t>(rd.get(4));
    if (size > (1ull << 40)) throw std::runtime_error("corrupted header");

    std::vector<uint8_t> file;
    if (mode == kModeStored) {
        if (comp.size() - rd.p != size) throw std::runtime_error("truncated compressed file");
        file.assign(comp.begin() + static_cast<long>(rd.p), comp.end());
        if (st) st->mode = "stored";
    } else if (mode == kModeBytes) {
        file.assign(size, 0);
        RangeDecoder dec(comp.data() + rd.p, comp.size() - rd.p);
        codeBytes(dec, file.data(), file.size());
        if (st) st->mode = "bytes";
    } else if (mode == kModeImage) {
        Image img;
        const uint64_t fmt = rd.get(1);
        if (fmt > 2) throw std::runtime_error("corrupted header");
        img.format = static_cast<Format>(fmt);
        img.channels = static_cast<int>(rd.get(1));
        img.bpp = static_cast<int>(rd.get(1));
        img.bigEndian = rd.get(1) != 0;
        img.width = static_cast<int>(rd.get(4));
        img.height = static_cast<int>(rd.get(4));
        img.maxval = static_cast<uint32_t>(rd.get(4));
        if ((img.channels != 1 && img.channels != 3) || (img.bpp != 8 && img.bpp != 16) || img.width <= 0 ||
            img.height <= 0 || img.maxval == 0 || img.maxval > 65535 ||
            img.numSamples() * (img.bpp / 8) > size)
            throw std::runtime_error("corrupted header");
        const uint64_t hdr = rd.get(4);
        if (hdr > comp.size() - rd.p) throw std::runtime_error("truncated compressed file");
        const uint64_t hdrPos = rd.p;
        rd.p += hdr;
        const uint64_t trailer = rd.get(8);
        const int t = static_cast<int>(rd.get(1));
        if (t >= kNumTransforms || (img.channels == 1 && t != kNone)) throw std::runtime_error("corrupted header");

        const int K = static_cast<int>(rd.get(2));
        if (K > 256) throw std::runtime_error("corrupted header");
        int gs[3];
        for (int& g : gs) {
            g = static_cast<int>(rd.get(1));
            if (g > 16) throw std::runtime_error("corrupted header");
        }
        const bool scaled = rd.get(1) != 0;
        int step[3] = {1, 1, 1}, off[3] = {0, 0, 0};
        uint32_t codedMax = img.maxval;
        if (scaled) {
            if (K > 0) throw std::runtime_error("corrupted header");
            codedMax = 0;
            for (int ch = 0; ch < 3; ++ch) {
                step[ch] = static_cast<int>(rd.get(2));
                off[ch] = static_cast<int>(rd.get(2));
                if (step[ch] < 1 || off[ch] >= step[ch] || static_cast<uint32_t>(off[ch]) > img.maxval)
                    throw std::runtime_error("corrupted header");
                if (ch < img.channels)
                    codedMax = std::max(codedMax, (img.maxval - static_cast<uint32_t>(off[ch])) /
                                                      static_cast<uint32_t>(step[ch]));
            }
        }
        const bool packed = rd.get(1) != 0;
        uint32_t packMin = 0, packBytes = 0;
        if (packed) {
            packMin = static_cast<uint32_t>(rd.get(4));
            packBytes = static_cast<uint32_t>(rd.get(4));
            if (img.channels != 1 || K > 0 || packBytes == 0 || packBytes > 8192 || packMin > codedMax)
                throw std::runtime_error("corrupted header");
        }
        const int profile = static_cast<int>(rd.get(1));
        const int matchOn = static_cast<int>(rd.get(1));
        if (matchOn > 1) throw std::runtime_error("corrupted header");
        if (profile > 2) throw std::runtime_error("corrupted header");

        // The output file is built in place: header, then each row of samples
        // as soon as it is decoded, then the trailer.
        const size_t bps = static_cast<size_t>(img.bpp / 8);
        const size_t rowBytes = static_cast<size_t>(img.width) * img.channels * bps;
        if (hdr + static_cast<uint64_t>(rowBytes) * img.height + trailer != size)
            throw std::runtime_error("corrupted header");
        file.assign(size, 0);
        std::copy_n(comp.begin() + static_cast<long>(hdrPos), hdr, file.begin());
        uint8_t* raster = file.data() + hdr;
        const bool be = img.format != Format::RAW || img.bigEndian;
        auto writeRow = [&](int ry, const uint16_t* v) {
            uint8_t* dst = raster + static_cast<size_t>(ry) * rowBytes;
            const size_t m = static_cast<size_t>(img.width) * img.channels;
            if (bps == 1) {
                for (size_t k = 0; k < m; ++k) dst[k] = static_cast<uint8_t>(v[k]);
            } else if (be) {
                for (size_t k = 0; k < m; ++k) {
                    dst[2 * k] = static_cast<uint8_t>(v[k] >> 8);
                    dst[2 * k + 1] = static_cast<uint8_t>(v[k]);
                }
            } else {
                for (size_t k = 0; k < m; ++k) {
                    dst[2 * k] = static_cast<uint8_t>(v[k]);
                    dst[2 * k + 1] = static_cast<uint8_t>(v[k] >> 8);
                }
            }
        };

        // streams: side information, then one per stripe
        const int S = static_cast<int>(rd.get(2));
        if (S < 1 || S > img.height) throw std::runtime_error("corrupted header");
        const uint64_t sideLen = rd.get(4);
        std::vector<uint64_t> lens(static_cast<size_t>(S));
        uint64_t total = sideLen;
        for (auto& l : lens) total += (l = rd.get(4));
        if (total > comp.size() - rd.p) throw std::runtime_error("truncated compressed file");
        const uint8_t* sp = comp.data() + rd.p;
        RangeDecoder dec(sp, sideLen);  // side stream (kept for the trailer)
        std::vector<const uint8_t*> sptr(static_cast<size_t>(S));
        {
            const uint8_t* q = sp + sideLen;
            for (int k = 0; k < S; ++k) {
                sptr[static_cast<size_t>(k)] = q;
                q += lens[static_cast<size_t>(k)];
            }
        }
        if (K > 0) {
            std::vector<uint8_t> pb(static_cast<size_t>(K) * img.channels * 2);
            codeBytes(dec, pb.data(), pb.size());
            std::vector<uint16_t> colours(static_cast<size_t>(K) * img.channels);
            for (size_t k = 0; k < colours.size(); ++k)
                colours[k] = static_cast<uint16_t>(pb[2 * k] | (pb[2 * k + 1] << 8));
            runParallel(S, threads, [&](int k) {
                RangeDecoder sd(sptr[static_cast<size_t>(k)], lens[static_cast<size_t>(k)]);
                std::vector<uint16_t> row(static_cast<size_t>(img.width) * img.channels);
                codeIndexRows(
                    sd, img.width, K, stripeStart(img.height, S, k), stripeStart(img.height, S, k + 1),
                    [](int, int32_t*) {},
                    [&](int ry, const int32_t* idx) {
                        for (int x = 0; x < img.width; ++x) {
                            if (idx[x] >= K) throw std::runtime_error("corrupted data");
                            for (int ch = 0; ch < img.channels; ++ch)
                                row[static_cast<size_t>(x) * img.channels + ch] =
                                    colours[static_cast<size_t>(idx[x]) * img.channels + ch];
                        }
                        writeRow(ry, row.data());
                    });
            });
        } else {
            Planes pl;
            pl.W = img.width;
            pl.H = img.height;
            pl.C = img.channels;
            transformRange(t, img.channels, static_cast<int>(codedMax), pl.lo, pl.hi);
            for (int ch = 0; ch < 3; ++ch) pl.gshift[ch] = gs[ch];
            pl.profile = profile;
            pl.match = matchOn;
            std::vector<int32_t> values;  // packed: rank -> value
            if (packed) {
                std::vector<uint8_t> map(packBytes);
                codeBytes(dec, map.data(), map.size());
                for (uint32_t b = 0; b < packBytes * 8; ++b)
                    if ((map[b >> 3] >> (b & 7)) & 1) values.push_back(static_cast<int32_t>(packMin + b));
                if (values.size() < 2 || values.back() > static_cast<int32_t>(codedMax))
                    throw std::runtime_error("corrupted data");
                pl.lo[0] = 0;
                pl.hi[0] = static_cast<int>(values.size()) - 1;
            }
            runParallel(S, threads, [&](int k) {
                RangeDecoder sd(sptr[static_cast<size_t>(k)], lens[static_cast<size_t>(k)]);
                const size_t m = static_cast<size_t>(img.width) * img.channels;
                std::vector<int32_t> tmp(m);
                std::vector<uint16_t> row(m);
                RowIO io;
                io.emit = [&](int ry, const int32_t* v) {
                    const int32_t* src = v;
                    if (packed) {
                        for (size_t q = 0; q < m; ++q) tmp[q] = values[static_cast<size_t>(v[q])];
                        src = tmp.data();
                    }
                    inverseTransform(t, img.channels, src, row.data(), static_cast<size_t>(img.width));
                    if (scaled)
                        for (size_t q = 0; q < m; ++q) {
                            const int ch = static_cast<int>(q % img.channels);
                            row[q] = static_cast<uint16_t>(row[q] * step[ch] + off[ch]);
                        }
                    writeRow(ry, row.data());
                };
                codeSamples(sd, pl, nullptr, stripeStart(img.height, S, k), stripeStart(img.height, S, k + 1), &io);
            });
        }
        if (trailer) codeBytes(dec, file.data() + hdr + rowBytes * img.height, trailer);
        if (st) {
            st->mode = "image";
            st->description = img.describe();
            st->pixels = img.numPixels();
        }
    } else {
        throw std::runtime_error("unknown mode " + std::to_string(mode));
    }

    if (file.size() != size || crc32(file.data(), file.size()) != crc)
        throw std::runtime_error("decoded file does not match the original (CRC mismatch): corrupted input or codec bug");
    if (st) {
        st->originalBytes = file.size();
        st->compressedBytes = comp.size();
    }
    return file;
}

}  // namespace ic
