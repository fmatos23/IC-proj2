# IC — Lab work 2: image coding

Information and Coding 2026/27, Universidade de Aveiro.

Report structure and progress: [relatorio/README.md](relatorio/README.md).

The test images (`imgs-pgm8/`, `imgs-ppm/`, `imgs-raw16/`) come from eLearning
and are not included in the repository; copy them to the repository root to run `make test`.

## Requirements

- Linux, `g++` with C++17 support, `make`
- For the histogram plots only: `python3` with `numpy` and `matplotlib`
  (`pip install numpy matplotlib`). Without them `img_hist` still prints the
  entropies and writes the CSV files.

## Build

```sh
make          # builds bin/img_hist, bin/img_quant, bin/img_cmp, bin/img_lossless_enc, bin/img_lossless_dec
make test     # sanity tests on the images in imgs-*/ and on generated special cases
make clean
```

## Image formats

| Format | Samples | Notes |
|---|---|---|
| PGM (`P5`, `P2`) | 8 or 16 bpp, 1 channel | 16 bpp when maxval > 255, big-endian |
| PPM (`P6`, `P3`) | 8 bpp per channel (16 also supported), R, G, B | |
| RAW | 8 or 16 bpp, 1 channel, no header | needs `-rows n -cols n [-bpp 8\|16] [-be\|-le]` (defaults: 8 bpp, big-endian) |

The test RAW files are named `...-u16be-1xROWSxCOLS.raw`, e.g.
`jkt...-u16be-1x1124x1124.raw` → `-rows 1124 -cols 1124 -bpp 16 -be`.

## Part I programs

### `img_hist` — histograms and entropies

```sh
bin/img_hist [-bits lo-hi] [-o prefix] [-noplot] [-show] [RAW options] input_img
```

For each channel (Y, or R, G, B), it prints the first-order entropy of:
- the sample values, `H(x)`;
- the horizontal differences `x[r][c] - x[r][c-1]`, `H(dx_h)`;
- the vertical differences `x[r][c] - x[r-1][c]`, `H(dx_v)`.

It writes `<prefix>_hist.csv`, `<prefix>_diff.csv`, `<prefix>_stats.csv` and
the plot `<prefix>_hist.png`. The default prefix is the input file name without
its extension, written to the current directory.

`-bits lo-hi` only uses bits `lo..hi` of each sample, i.e. `(x >> lo) & (2^(hi-lo+1)-1)`.

```sh
bin/img_hist imgs-ppm/lena.ppm
bin/img_hist -bits 0-5 imgs-pgm8/lena.pgm          # six least significant bits
bin/img_hist -bits 4-7 imgs-pgm8/barbara.pgm -o barbara_msb
bin/img_hist -rows 1124 -cols 1124 -bpp 16 -be imgs-raw16/jkt19980403_00032950_img0-u16be-1x1124x1124.raw
```

### `img_quant` — uniform scalar quantization

```sh
bin/img_quant -b bits [-index] [-floor] [RAW options] input_img output_img
```

Each sample is quantized to `L = 2^bits` uniformly spaced levels. The output
has the same format as the input.
- By default, it writes the reconstructed values (the mid-point of each
  interval) in the original range, so the result can be viewed and compared
  with `img_cmp`.
- `-floor` reconstructs to the lower end of each interval (plain truncation of
  the least significant bits).
- `-index` writes the quantization indices themselves (maxval `2^bits-1`).

```sh
bin/img_quant -b 4 imgs-ppm/lena.ppm lena_4bit.ppm
bin/img_quant -b 10 -rows 1124 -cols 1124 -bpp 16 -be in.raw out.raw
```

### `img_cmp` — error and PSNR

```sh
bin/img_cmp [-peak v] [RAW options] original_img other_img
```

Prints, per channel and overall, the maximum absolute error, the MSE and the
`PSNR = 10·log10(peak² / MSE)`. The peak defaults to the original's maxval
(255 for 8-bit images, 65535 for 16-bit RAW).

```sh
bin/img_cmp imgs-ppm/lena.ppm lena_4bit.ppm
```

## Part II: lossless codec

```sh
bin/img_lossless_enc [-v] input_img compressed_file                                   # PGM / PPM
bin/img_lossless_enc [-v] -rows n -cols n input_img compressed_file                   # 8 bpp RAW
bin/img_lossless_enc [-v] -rows n -cols n -bpp 16 [-be|-le] input_img compressed_file # 16 bpp RAW
bin/img_lossless_dec [-v] [-t n] compressed_file output_img
```

Encoder options (all optional; the default gives the best compression):

| Option | Effect (measured on the course images) |
|---|---|
| `-fast` | ~15% faster decoding, ~35% faster encoding, files 1–2% larger |
| `-fastest` | ~40% faster, files 4–6% larger |
| `-t n` | splits the image into n stripes of rows coded independently in parallel (encoder and decoder); files 0.1–1% larger with 2 stripes (more for palette images); 2 threads: ~1.9× faster decoding |

The decoder needs no options: it reads the profile and the number of stripes
from the file and uses all cores for files with stripes (`-t n` limits them).

The decoded file is byte-for-byte identical to the input, including PNM
comments and any bytes after the samples. The decoder needs no options: the
format, dimensions, byte order and original header are stored in the
compressed file, together with a CRC-32 of the original that the decoder
checks. `-v` prints the mode used, the estimated bits per sample of each
channel and the time.

How it works:
- **Colour (PPM):** a reversible colour transform (none, G/R−G/B−G or the
  JPEG 2000 RCT) is chosen per image by coding a sample of the rows with each
  one (dry run of the real coder) and keeping the cheapest.
- **Palette mode:** images with at most 256 colours (or grey levels) are coded
  as a palette plus an index image when that is cheaper on the sample; the
  indices are coded with context models of the neighbouring indices mixed by
  logistic mixing (dithered palette images defeat pixel prediction).
- **Value scaling:** if all the values of a channel are `off + k·step` (e.g.
  12-bit data in the top bits of 16-bit samples, common in cinema and medical
  images), `k` is coded instead (step and offset per channel in the header).
- **Value packing (grey images):** if only some of the values between the
  minimum and the maximum occur, the rank of each value is coded instead (the
  set of values used is sent as a bitmap), when that is cheaper on the sample.
- **Prediction:** a blend of 9 predictors, weighted by 1/(recent error of each
  predictor at W, N, NW, NE), with 3 fractional bits: W, N, W+N−NW, W+NE−N,
  2N−NN, two adaptive linear predictors trained online with normalised LMS
  (12 taps: 9 causal neighbours and, for colour channels 1–2, the previous
  channels at the same pixel; fast and slow learning rates) and, for channels
  1–2, two "same gradient as the previous channel" predictors. A second-stage
  NLMS predicts the remaining error from the neighbouring residuals (it
  replaced the JPEG-LS-style bias correction, which then hurt). For channels
  1–2 an adaptive fraction of the channel-0 error is also added. The residual
  is reduced modulo the sample range.
- **Entropy coding:** the residual is binarised (zero flag, sign, magnitude
  class in unary, mantissa bits); each decision is coded by a binary adaptive
  arithmetic coder. Zero flag and class bits mix (logistic mixing) two models
  with different contexts (local activity + recent residuals + spread of the
  predictions; expected error of the blend + channel-0 residual); the fractional part of the prediction is
  also used as context for the zero flag and the sign. Probabilities adapt with
  a rate that starts at 1/(n+1.5) and settles at 1/256.
- **Match model (synthetic images):** a hash table remembers, for each context
  of exact neighbouring pixels (3 window sizes), the pixel that followed it
  (and the one before that); when the context repeats, one binary decision
  says whether the pixel is the remembered one, and if so its residuals are not
  coded. It is enabled per image only if it saves at least 1% on the sample
  (text, charts and screenshots; photographs almost never), so it costs no
  time on photographs.
- **Fallbacks:** files that cannot be rebuilt from their samples (ASCII PNM,
  samples above maxval, non-image files) are compressed as generic bytes
  (order-1 context model with the same coder); incompressible files are stored.

Memory: the image is never held as a whole. The encoder reads the samples
straight from the input file bytes and builds each transformed row when it is
needed; the coder keeps only the last 3 rows; the decoder writes each decoded
row straight into the output file buffer. Peak memory is about the size of
the input plus the output file (~5 bytes/pixel for 8-bit PPM).

All arithmetic that the decoder must repeat is integer, so the bitstream does
not depend on the machine or the compiler flags. For experiments only, the
environment variables `IC_TUNE`, `IC_TRANSFORM`, `IC_PALETTE`, `IC_TCAND`,
`IC_MIXRATE` and `IC_MIXINIT` override encoder choices (see `codec.cpp`).

Source: `src/lossless/` (`range_coder.hpp`, `model.hpp`, `predictor.hpp`,
`palette.hpp`, `transform.{hpp,cpp}`, `crc32.hpp`, `codec.{hpp,cpp}`), shared by `src/img_lossless_enc.cpp` and
`src/img_lossless_dec.cpp`. Encoder and decoder run the same templated code
(`codeSamples`), so they cannot drift apart.

## Part II.5: lossless state-of-the-art benchmark

`scripts/benchmark_sota.py` compresses every test image with existing lossless
codecs and measures bpp, encoding/decoding time and peak memory (single thread),
checking that decoding is exact. Results go to `results/`:
`sota_<host>.csv` (one row per image and codec), `sota_<host>_summary.csv`
(averages per group and codec) and `sota_<host>.md` (tables). The summary has the
mean bpp, the size relative to JPEG-LS (geometric mean of the per-image ratios,
comparable across groups), wins, throughput in Mpix/s and peak memory.
`--from-csv results/sota_<host>.csv` rebuilds the summaries without running anything.

| Codec | Tool | Applied to |
|---|---|---|
| JPEG-LS | CharLS through `bin/charls_cli` (Ubuntu has the library, not a CLI) | PNM |
| JPEG-XL lossless | `cjxl -d 0 -e 1/3/7/9` | PNM |
| PNG | `optipng -o2` (decoded with `pngtopnm`) | PNM |
| zstd / xz / bzip2 | `zstd -19`, `xz -9`, `bzip2 -9` | original file |
| ours | `bin/img_lossless_enc` / `img_lossless_dec` | original file (RAW options from the file name) |

16-bit RAW images are converted to 16-bit PGM for the image codecs, and the
gzipped Kodak images (`imgs-ppm/kodak/`) are decompressed first.

```sh
sudo apt install libcharls-dev libjxl-tools optipng netpbm zstd xz-utils bzip2 time
make bench                                         # all images, 3 runs each (slow: JPEG-XL -e 9)
python3 scripts/benchmark_sota.py --skip-slow      # without JPEG-XL -e 9
python3 scripts/benchmark_sota.py --groups pgm8 ppm --repeat 1
# hold-out set (images never used to tune the codec), see results/AVALIACAO.md:
python3 scripts/get_holdout_images.py /tmp/holdout
# synthetic images (charts, text, screenshots), split in dev/ and test/:
python3 scripts/get_synthetic_images.py /tmp/synth
# real 16-bit grey (medical) and colour (cinema 2K/4K) images and large photos:
python3 scripts/get_more_images.py /tmp/more
# only our codec, merged with an earlier run of the reference codecs:
python3 scripts/benchmark_sota.py --codecs ours --merge results/sota_<host>.csv --tag ours
```

## Repository layout

```
src/common/image.{hpp,cpp}   image I/O (RAW/PGM/PPM) and shared CLI parsing
src/img_hist.cpp             Part I.1
src/img_quant.cpp            Part I.2
src/img_cmp.cpp              Part I.3
src/lossless/                Part II codec (shared by encoder and decoder)
src/img_lossless_{enc,dec}.cpp  Part II programs
scripts/img_hist_plot.py     plotting helper called by img_hist (copied to bin/)
tests/run_tests.sh           sanity tests (make test)
tests/make_special_images.py special-case files for the lossless tests
tools/charls_cli.cpp         JPEG-LS command line front-end (benchmark only)
scripts/benchmark_sota.py    Part II.5 benchmark of existing codecs
scripts/get_holdout_images.py  separate test set (never used for tuning)
scripts/get_synthetic_images.py synthetic images (dev/ for design, test/ for measuring)
scripts/get_more_images.py     real 16-bit grey/colour images and large photos (point 4)
results/                     benchmark results (AVALIACAO.md: honest evaluation)
```
