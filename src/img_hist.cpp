// img_hist - histograms and first-order entropies of RAW/PGM/PPM images.
//
// For every channel it computes the histogram of the sample values and of the
// horizontal (x[r][c] - x[r][c-1]) and vertical (x[r][c] - x[r-1][c])
// differences between neighbouring pixels, prints the corresponding entropies
// and writes the histograms to CSV files. A plot (PNG) is produced by the
// companion script img_hist_plot.py.
//
// With -bits lo-hi only the bits lo..hi of each sample are considered, i.e.
// the value (x >> lo) & (2^(hi-lo+1) - 1).

#include <sys/wait.h>
#include <unistd.h>

#include <climits>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "common/image.hpp"

using namespace ic;

namespace {

void usage() {
    std::cerr <<
        "Usage: img_hist [options] input_img\n"
        "\n"
        "Options:\n"
        "  -bits lo-hi   only consider bits lo..hi of each sample (default: all)\n"
        "  -o prefix     prefix for output files (default: input name without extension)\n"
        "  -noplot       do not write CSV files nor the PNG plot\n"
        "  -show         also open an interactive plot window\n"
        "RAW images:\n"
        "  -rows n -cols n [-bpp 8|16] [-be|-le]   (default 8 bpp, big-endian)\n";
}

struct ChannelStats {
    std::vector<uint64_t> hist;   // size L
    std::vector<uint64_t> hdiff;  // size 2L-1, index d + (L-1)
    std::vector<uint64_t> vdiff;
    double H = 0, Hh = 0, Hv = 0;
    uint32_t minv = 0, maxv = 0;
    size_t used = 0;  // number of distinct values
};

double entropy(const std::vector<uint64_t>& h) {
    uint64_t total = 0;
    for (uint64_t c : h) total += c;
    if (total == 0) return 0.0;
    double H = 0.0;
    const double inv = 1.0 / static_cast<double>(total);
    for (uint64_t c : h)
        if (c) {
            double p = c * inv;
            H -= p * std::log2(p);
        }
    return H;
}

ChannelStats computeStats(const Image& img, int ch, int lo, int hi) {
    const int nbits = hi - lo + 1;
    const uint32_t L = 1u << nbits;
    const uint32_t mask = L - 1;
    const int off = static_cast<int>(L) - 1;

    ChannelStats s;
    s.hist.assign(L, 0);
    s.hdiff.assign(2 * L - 1, 0);
    s.vdiff.assign(2 * L - 1, 0);

    const int W = img.width, Hgt = img.height, C = img.channels;
    std::vector<int> prev(W), cur(W);
    for (int r = 0; r < Hgt; ++r) {
        const uint16_t* row = img.data.data() + static_cast<size_t>(r) * W * C + ch;
        for (int c = 0; c < W; ++c) cur[c] = (row[static_cast<size_t>(c) * C] >> lo) & mask;
        for (int c = 0; c < W; ++c) {
            ++s.hist[cur[c]];
            if (c > 0) ++s.hdiff[cur[c] - cur[c - 1] + off];
            if (r > 0) ++s.vdiff[cur[c] - prev[c] + off];
        }
        std::swap(prev, cur);
    }

    s.H = entropy(s.hist);
    s.Hh = entropy(s.hdiff);
    s.Hv = entropy(s.vdiff);
    s.minv = UINT32_MAX;
    for (uint32_t v = 0; v < L; ++v)
        if (s.hist[v]) {
            if (s.minv == UINT32_MAX) s.minv = v;
            s.maxv = v;
            ++s.used;
        }
    return s;
}

void writeCSVs(const std::string& prefix, const Image& img, const std::vector<ChannelStats>& st,
               int lo, int hi) {
    const int C = img.channels;
    const size_t L = st[0].hist.size();
    const long off = static_cast<long>(L) - 1;

    std::string path = prefix + "_hist.csv";
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) throw std::runtime_error("cannot create " + path);
    std::fprintf(f, "value");
    for (int ch = 0; ch < C; ++ch) std::fprintf(f, ",%s", img.channelName(ch).c_str());
    std::fprintf(f, "\n");
    for (size_t v = 0; v < L; ++v) {
        std::fprintf(f, "%zu", v);
        for (int ch = 0; ch < C; ++ch) std::fprintf(f, ",%llu", (unsigned long long)st[ch].hist[v]);
        std::fprintf(f, "\n");
    }
    std::fclose(f);

    path = prefix + "_diff.csv";
    f = std::fopen(path.c_str(), "w");
    if (!f) throw std::runtime_error("cannot create " + path);
    std::fprintf(f, "diff");
    for (int ch = 0; ch < C; ++ch) {
        std::string n = img.channelName(ch);
        std::fprintf(f, ",%s_h,%s_v", n.c_str(), n.c_str());
    }
    std::fprintf(f, "\n");
    for (size_t i = 0; i < 2 * L - 1; ++i) {
        std::fprintf(f, "%ld", static_cast<long>(i) - off);
        for (int ch = 0; ch < C; ++ch)
            std::fprintf(f, ",%llu,%llu", (unsigned long long)st[ch].hdiff[i],
                         (unsigned long long)st[ch].vdiff[i]);
        std::fprintf(f, "\n");
    }
    std::fclose(f);

    path = prefix + "_stats.csv";
    f = std::fopen(path.c_str(), "w");
    if (!f) throw std::runtime_error("cannot create " + path);
    std::fprintf(f, "channel,H,H_hdiff,H_vdiff,bit_lo,bit_hi\n");
    for (int ch = 0; ch < C; ++ch)
        std::fprintf(f, "%s,%.6f,%.6f,%.6f,%d,%d\n", img.channelName(ch).c_str(), st[ch].H, st[ch].Hh,
                     st[ch].Hv, lo, hi);
    std::fclose(f);
}

// Directory containing this executable (the plot script is installed there).
std::string selfDir() {
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n <= 0) return ".";
    buf[n] = '\0';
    std::string p(buf);
    size_t slash = p.rfind('/');
    return slash == std::string::npos ? "." : p.substr(0, slash);
}

bool runPlotScript(const std::string& prefix, const std::string& title, bool show) {
    std::string script = selfDir() + "/img_hist_plot.py";
    if (access(script.c_str(), R_OK) != 0) {
        std::cerr << "warning: plot script not found at " << script << "\n";
        return false;
    }
    std::vector<std::string> args = {"python3", script, prefix, "--title", title};
    if (show) args.push_back("--show");
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    std::fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        execvp(argv[0], argv.data());
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

std::string defaultPrefix(const std::string& input) {
    size_t slash = input.rfind('/');
    std::string base = slash == std::string::npos ? input : input.substr(slash + 1);
    size_t dot = base.rfind('.');
    return dot == std::string::npos || dot == 0 ? base : base.substr(0, dot);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        RawParams raw;
        std::string input, prefix;
        int lo = -1, hi = -1;
        bool plot = true, show = false;

        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (parseRawOption(argc, argv, i, raw)) continue;
            if (a == "-h" || a == "--help") { usage(); return 0; }
            if (a == "-bits") {
                if (++i >= argc) throw std::runtime_error("missing value for -bits");
                if (std::sscanf(argv[i], "%d-%d", &lo, &hi) != 2 || lo < 0 || hi < lo)
                    throw std::runtime_error("-bits expects lo-hi, e.g. 0-5");
            } else if (a == "-o") {
                if (++i >= argc) throw std::runtime_error("missing value for -o");
                prefix = argv[i];
            } else if (a == "-noplot") plot = false;
            else if (a == "-show") show = true;
            else if (!a.empty() && a[0] == '-') throw std::runtime_error("unknown option " + a);
            else if (input.empty()) input = a;
            else throw std::runtime_error("unexpected argument " + a);
        }
        if (input.empty()) { usage(); return 1; }

        Image img = readImage(input, raw);
        const int bits = img.sampleBits();
        if (lo < 0) { lo = 0; hi = bits - 1; }
        if (hi >= img.bpp)
            throw std::runtime_error("bit range exceeds the " + std::to_string(img.bpp) + " bits of the samples");

        std::vector<ChannelStats> st;
        for (int ch = 0; ch < img.channels; ++ch) st.push_back(computeStats(img, ch, lo, hi));

        std::printf("Image     : %s\n", input.c_str());
        std::printf("Format    : %s\n", img.describe().c_str());
        std::printf("Bit range : %d-%d (%d bits, alphabet size %u)\n", lo, hi, hi - lo + 1, 1u << (hi - lo + 1));
        std::printf("\nFirst-order entropies (bits/symbol)\n");
        std::printf("%-8s %10s %10s %10s %10s %12s\n", "channel", "H(x)", "H(dx_h)", "H(dx_v)", "min-max",
                    "#distinct");
        for (int ch = 0; ch < img.channels; ++ch) {
            char range[32];
            std::snprintf(range, sizeof range, "%u-%u", st[ch].minv, st[ch].maxv);
            std::printf("%-8s %10.4f %10.4f %10.4f %10s %12zu\n", img.channelName(ch).c_str(), st[ch].H,
                        st[ch].Hh, st[ch].Hv, range, st[ch].used);
        }

        if (plot) {
            if (prefix.empty()) {
                prefix = defaultPrefix(input);
                if (lo != 0 || hi != bits - 1) prefix += "_bits" + std::to_string(lo) + "-" + std::to_string(hi);
            }
            writeCSVs(prefix, img, st, lo, hi);
            std::string title = defaultPrefix(input) + "  (bits " + std::to_string(lo) + "-" + std::to_string(hi) + ")";
            bool ok = runPlotScript(prefix, title, show);
            std::printf("\nWrote %s_hist.csv, %s_diff.csv, %s_stats.csv%s\n", prefix.c_str(), prefix.c_str(),
                        prefix.c_str(), ok ? (", " + prefix + "_hist.png").c_str() : "");
            if (!ok) std::fprintf(stderr, "warning: PNG plot was not produced (needs python3 + matplotlib)\n");
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "img_hist: " << e.what() << "\n";
        return 1;
    }
}
