// Palette mode: for images with few distinct colours (or grey levels), the
// image is coded as a palette plus an image of palette indices.
//
// Dithered palette images defeat pixel prediction (neighbouring pixels often
// have quite different colours), but their index patterns repeat, so the
// indices are coded with context models instead: each index is written as a
// sequence of binary decisions along a binary tree, and the probability of
// each decision is a mix (logistic mixing) of models conditioned on one
// neighbouring index each (W, N, NW, NE) and of an order-0 model. (WW, NN
// and hashed pairs of neighbours were tried and did not help.)
#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "lossless/model.hpp"

namespace ic {

class PaletteModel {
public:
    static constexpr int kNbr = 4;  // W, N, NW, NE

    explicit PaletteModel(int K)
        : bits_(K > 1 ? bitLength(static_cast<uint32_t>(K - 1)) : 1), mixer_(kNbr + 1, 256 * 3, 24) {
        for (auto& t : ctx_) t.resize(256 * 256);
        o0_.resize(256);
    }

    template <class Coder>
    int code(Coder& c, int sym, const int nbr[kNbr]) {
        const int eq = (nbr[0] == nbr[1]) + (nbr[0] == nbr[2]) * 2 > 2 ? 2 : (nbr[0] == nbr[1]) + (nbr[0] == nbr[2]) * 2;
        uint32_t node = 1;
        BitModel* m[kNbr + 1];
        for (int b = bits_ - 1; b >= 0; --b) {
            for (int k = 0; k < kNbr; ++k) m[k] = &ctx_[k][static_cast<size_t>(nbr[k]) * 256 + node];
            m[kNbr] = &o0_[node];
            for (int k = 0; k <= kNbr; ++k) mixer_.add(stretch(p1of(*m[k])));
            const int p1 = mixer_.mix(static_cast<int>(node) * 3 + eq);
            const int bit = c.bit(p0from12(p1), (sym >> b) & 1);
            mixer_.update(bit);
            for (int k = 0; k <= kNbr; ++k) m[k]->update(bit);
            node = (node << 1) | static_cast<uint32_t>(bit);
        }
        return static_cast<int>(node - (1u << bits_));
    }

private:
    int bits_;
    std::vector<BitModel> ctx_[kNbr], o0_;
    Mixer mixer_;
};

// Codes rows r0 .. r1-1 of an index image (values < K), as an independent
// stripe (nothing above r0 is used). The encoder gets each row with
// fill(row, dst); the decoder hands each decoded row to emit(row, src). Only
// two rows are kept in memory.
template <class Coder, class Fill, class Emit>
void codeIndexRows(Coder& c, int W, int K, int r0, int r1, Fill fill, Emit emit) {
    PaletteModel m(K);
    std::vector<int32_t> ring(2 * static_cast<size_t>(W));
    for (int ry = r0; ry < r1; ++ry) {
        int32_t* cur = &ring[static_cast<size_t>((ry - r0) & 1) * W];
        const int32_t* up = ry > r0 ? &ring[static_cast<size_t>((ry - r0 + 1) & 1) * W] : nullptr;
        if (Coder::kEncoding) fill(ry, cur);
        for (int x = 0; x < W; ++x) {
            int n[PaletteModel::kNbr];
            n[1] = up ? up[x] : (x > 0 ? cur[x - 1] : 0);         // N
            n[0] = x > 0 ? cur[x - 1] : n[1];                       // W
            n[2] = up && x > 0 ? up[x - 1] : n[1];                  // NW
            n[3] = up && x + 1 < W ? up[x + 1] : n[1];              // NE
            cur[x] = m.code(c, Coder::kEncoding ? cur[x] : 0, n);
        }
        if (!Coder::kEncoding) emit(ry, cur);
    }
}

// Same, for an index image in memory (row-major).
template <class Coder>
void codeIndices(Coder& c, std::vector<int32_t>& idx, int W, int K, int r0, int r1) {
    int32_t* base = idx.data();
    codeIndexRows(
        c, W, K, r0, r1, [&](int ry, int32_t* dst) { std::copy_n(base + static_cast<size_t>(ry) * W, W, dst); },
        [&](int ry, const int32_t* src) { std::copy_n(src, W, base + static_cast<size_t>(ry) * W); });
}

}  // namespace ic
