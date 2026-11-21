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
make          # builds bin/img_hist, bin/img_quant, bin/img_cmp
make test     # runs sanity tests on the images in imgs-*/
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

## Part II.5: lossless state-of-the-art benchmark

`scripts/benchmark_sota.py` compresses every test image with existing lossless
codecs and measures bpp, encoding/decoding time and peak memory (single thread),
checking that decoding is exact. Results go to `results/sota_<host>.{csv,md}`.

| Codec | Tool | Applied to |
|---|---|---|
| JPEG-LS | CharLS through `bin/charls_cli` (Ubuntu has the library, not a CLI) | PNM |
| JPEG-XL lossless | `cjxl -d 0 -e 1/3/7/9` | PNM |
| PNG | `optipng -o2` (decoded with `pngtopnm`) | PNM |
| zstd / xz / bzip2 | `zstd -19`, `xz -9`, `bzip2 -9` | original file |

16-bit RAW images are converted to 16-bit PGM for the image codecs, and the
gzipped Kodak images (`imgs-ppm/kodak/`) are decompressed first.

```sh
sudo apt install libcharls-dev libjxl-tools optipng netpbm zstd xz-utils bzip2 time
make bench                                         # all images, 3 runs each (slow: JPEG-XL -e 9)
python3 scripts/benchmark_sota.py --skip-slow      # without JPEG-XL -e 9
python3 scripts/benchmark_sota.py --groups pgm8 ppm --repeat 1
```

## Repository layout

```
src/common/image.{hpp,cpp}   image I/O (RAW/PGM/PPM) and shared CLI parsing
src/img_hist.cpp             Part I.1
src/img_quant.cpp            Part I.2
src/img_cmp.cpp              Part I.3
scripts/img_hist_plot.py     plotting helper called by img_hist (copied to bin/)
tests/run_tests.sh           sanity tests (make test)
tools/charls_cli.cpp         JPEG-LS command line front-end (benchmark only)
scripts/benchmark_sota.py    Part II.5 benchmark of existing codecs
results/                     benchmark results
```
