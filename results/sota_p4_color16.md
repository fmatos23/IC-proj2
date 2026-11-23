# Lossless state-of-the-art benchmark

Machine: vm, Intel(R) Xeon(R) Processor @ 2.10GHz, 2 CPUs, Linux 6.18.44-fc-v726  
Times: minimum of 3 run(s) (JPEG-XL -e 9: 1 run), single thread, wall clock, including file I/O.  
Versions: cjxl: cjxl v0.7.0 [AVX3_SPR,AVX3,AVX2,SSE4,SSSE3,SSE2]; optipng: OptiPNG version 0.7.8; zstd: *** Zstandard CLI (64-bit) v1.5.5, by Yann Collet ***; xz: xz (XZ Utils) 5.4.5; bzip2: bzip2, a block-sorting file compressor.  Version 1.0.8, 13-Jul-2019.
Rows of codecs not run now taken from sota_p4_color16.csv.  

bpp = compressed bits / number of pixels (a PPM pixel has 3 samples: raw size 24 bpp).  
mean bpp: every image weighs the same; total bpp: all bits / all pixels.  
vs JPEG-LS: geometric mean of the per-image size ratio to JPEG-LS (negative = smaller).  
Mpix/s: megapixels per second (all pixels / total time).

## PPM 24 bpp (4 images)

| Codec | mean bpp | total bpp | vs JPEG-LS | wins | enc Mpix/s | dec Mpix/s | max mem enc / dec (MB) |
|---|---|---|---|---|---|---|---|
| ours | 25.653 | 21.694 | -25.8% | 2/4 | 0.93 | 1.06 | 107 / 75 |
| ours-fast | 26.051 | 22.431 | -24.3% | 0/4 | 1.34 | 1.50 | 108 / 76 |
| ours-mt | 25.653 | 21.697 | -25.8% | 2/4 | 1.38 | 1.96 | 137 / 77 |
| jpeg-ls | 33.151 | 30.537 | +0.0% | 0/4 | 4.92 | 6.29 | 155 / 167 |
| jpeg-xl-e7 | 27.326 | 24.196 | -20.3% | 0/4 | 0.21 | 2.49 | 1092 / 237 |
| jpeg-xl-e9 | 27.191 | 24.008 | -20.8% | 0/4 | 0.01 | 2.38 | 2045 / 237 |
| png-optipng | 36.421 | 35.407 | +9.4% | 0/4 | 0.39 | 12.32 | 53 / 54 |
| zstd-19 | 41.781 | 42.401 | +24.5% | 0/4 | 0.50 | 69.23 | 164 / 13 |
| xz-9 | 39.255 | 38.984 | +16.6% | 0/4 | 0.32 | 3.16 | 522 / 53 |
| bzip2-9 | 39.453 | 40.093 | +16.3% | 0/4 | 1.99 | 1.99 | 8 / 5 |

bpp per image:

| Image | ours | ours-fast | ours-mt | jpeg-ls | jpeg-xl-e7 | jpeg-xl-e9 | png-optipng | zstd-19 | xz-9 | bzip2-9 |
|---|---|---|---|---|---|---|---|---|---|---|
| ElephantDream_4K.ppm | **17.365** | 18.510 | 17.370 | 27.546 | 20.885 | 20.643 | 34.555 | 43.604 | 39.151 | 41.365 |
| X_4_2K_24_185_CBR_WB_000.ppm | 32.559 | 32.733 | **32.554** | 34.489 | 34.027 | 33.812 | 40.140 | 47.031 | 45.011 | 46.195 |
| X_5_2K_24_235_CBR_STEM24_000.ppm | 34.195 | 34.372 | **34.189** | 37.241 | 35.581 | 35.630 | 42.100 | 47.323 | 45.930 | 45.443 |
| X_6_2K_24_FULL_CBR_CIRCLE_000.ppm | **18.495** | 18.588 | 18.499 | 33.327 | 18.813 | 18.681 | 28.889 | 29.168 | 26.926 | 24.809 |
