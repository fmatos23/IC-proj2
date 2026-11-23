// Pixel prediction from already coded neighbours.
//
//     NW  N  NE
//     W   x
#pragma once

#include <cstdint>
#include <cstdlib>

namespace ic {

struct Neighbours {
    int W, N, NW, NE;
};

// MED / LOCO-I predictor (JPEG-LS): picks W or N at an edge, otherwise the
// planar prediction W + N - NW.
inline int predictMED(const Neighbours& n) {
    const int mx = n.W > n.N ? n.W : n.N;
    const int mn = n.W > n.N ? n.N : n.W;
    if (n.NW >= mx) return mn;
    if (n.NW <= mn) return mx;
    return n.W + n.N - n.NW;
}

// Local activity: sum of absolute gradients around the pixel.
inline int activity(const Neighbours& n) {
    return std::abs(n.N - n.NW) + std::abs(n.W - n.NW) + std::abs(n.NE - n.N);
}

}  // namespace ic
