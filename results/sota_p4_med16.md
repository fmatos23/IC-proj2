# Lossless state-of-the-art benchmark

Machine: vm, Intel(R) Xeon(R) Processor @ 2.10GHz, 2 CPUs, Linux 6.18.44-fc-v726  
Times: minimum of 3 run(s) (JPEG-XL -e 9: 1 run), single thread, wall clock, including file I/O.  
Versions: cjxl: cjxl v0.7.0 [AVX3_SPR,AVX3,AVX2,SSE4,SSSE3,SSE2]; optipng: OptiPNG version 0.7.8; zstd: *** Zstandard CLI (64-bit) v1.5.5, by Yann Collet ***; xz: xz (XZ Utils) 5.4.5; bzip2: bzip2, a block-sorting file compressor.  Version 1.0.8, 13-Jul-2019.
Rows of codecs not run now taken from sota_p4_med16.csv.  

bpp = compressed bits / number of pixels (a PPM pixel has 3 samples: raw size 24 bpp).  
mean bpp: every image weighs the same; total bpp: all bits / all pixels.  
vs JPEG-LS: geometric mean of the per-image size ratio to JPEG-LS (negative = smaller).  
Mpix/s: megapixels per second (all pixels / total time).

## RAW 16 bpp (5 images)

| Codec | mean bpp | total bpp | vs JPEG-LS | wins | enc Mpix/s | dec Mpix/s | max mem enc / dec (MB) |
|---|---|---|---|---|---|---|---|
| ours | 4.032 | 5.472 | -13.5% | 3/5 | 3.57 | 3.69 | 23 / 15 |
| ours-fast | 4.118 | 5.523 | -11.0% | 0/5 | 4.96 | 5.78 | 23 / 15 |
| ours-mt | 4.037 | 5.471 | -13.3% | 1/5 | 4.64 | 7.05 | 27 / 16 |
| jpeg-ls | 4.467 | 5.687 | +0.0% | 0/5 | 14.34 | 23.86 | 26 / 23 |
| jpeg-xl-e7 | 4.189 | 5.583 | -9.3% | 0/5 | 0.93 | 7.76 | 450 / 69 |
| jpeg-xl-e9 | 4.141 | 5.532 | -10.7% | 1/5 | 0.04 | 8.27 | 450 / 72 |
| png-optipng | 5.986 | 7.336 | +37.8% | 0/5 | 1.43 | 17.14 | 10 / 10 |
| zstd-19 | 7.028 | 8.832 | +62.5% | 0/5 | 1.60 | 101.89 | 97 / 11 |
| xz-9 | 6.026 | 7.579 | +37.6% | 0/5 | 1.37 | 15.14 | 128 / 9 |
| bzip2-9 | 5.309 | 6.601 | +21.0% | 0/5 | 8.28 | 10.26 | 8 / 5 |

bpp per image:

| Image | ours | ours-fast | ours-mt | jpeg-ls | jpeg-xl-e7 | jpeg-xl-e9 | png-optipng | zstd-19 | xz-9 | bzip2-9 |
|---|---|---|---|---|---|---|---|---|---|---|
| 693_UNCR-u16be-1x512x512.raw | **2.423** | 2.480 | 2.432 | 3.047 | 2.486 | 2.455 | 4.491 | 5.278 | 4.432 | 3.888 |
| MR-SIEMENS-DICOM-WithOverlays-u16be-1x484x484.raw | **2.331** | 2.464 | 2.344 | 3.132 | 2.688 | 2.653 | 4.521 | 5.305 | 4.535 | 3.964 |
| MR2_UNCR-u16le-1x1024x1024.raw | **4.115** | 4.298 | 4.120 | 4.576 | 4.310 | 4.229 | 6.062 | 6.320 | 5.603 | 5.275 |
| RG1_UNCR-u16be-1x1955x1841.raw | 9.216 | 9.233 | **9.210** | 9.327 | 9.378 | 9.350 | 11.687 | 14.059 | 12.293 | 10.687 |
| RG3_UNCR-u16be-1x1760x1760.raw | 2.077 | 2.116 | 2.077 | 2.250 | 2.085 | **2.016** | 3.166 | 4.175 | 3.268 | 2.733 |
