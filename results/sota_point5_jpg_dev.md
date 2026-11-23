# Lossless state-of-the-art benchmark

Machine: vm, Intel(R) Xeon(R) Processor @ 2.10GHz, 2 CPUs, Linux 6.18.44-fc-v726  
Times: minimum of 1 run(s) (JPEG-XL -e 9: 1 run), single thread, wall clock, including file I/O.  
Versions: cjxl: cjxl v0.7.0 [AVX3_SPR,AVX3,AVX2,SSE4,SSSE3,SSE2]; optipng: OptiPNG version 0.7.8; zstd: *** Zstandard CLI (64-bit) v1.5.5, by Yann Collet ***; xz: xz (XZ Utils) 5.4.5; bzip2: bzip2, a block-sorting file compressor.  Version 1.0.8, 13-Jul-2019.
Rows of codecs not run now taken from sota_jpg_dev.csv.  

bpp = compressed bits / number of pixels (a PPM pixel has 3 samples: raw size 24 bpp).  
mean bpp: every image weighs the same; total bpp: all bits / all pixels.  
vs JPEG-LS: geometric mean of the per-image size ratio to JPEG-LS (negative = smaller).  
Mpix/s: megapixels per second (all pixels / total time).

## PGM 8 bpp (2 images)

| Codec | mean bpp | total bpp | vs JPEG-LS | wins | enc Mpix/s | dec Mpix/s | max mem enc / dec (MB) |
|---|---|---|---|---|---|---|---|
| ours | 4.688 | 4.688 | -5.0% | 2/2 | 1.45 | 3.56 | 7 / 5 |
| jpeg-ls | 4.895 | 4.895 | +0.0% | 0/2 | 16.07 | 20.83 | 5 / 5 |
| jpeg-xl-e7 | 4.767 | 4.767 | -2.9% | 0/2 | 1.05 | 5.07 | 43 / 13 |
| jpeg-xl-e9 | 4.753 | 4.753 | -3.6% | 0/2 | 0.06 | 6.46 | 48 / 13 |

bpp per image:

| Image | ours | jpeg-ls | jpeg-xl-e7 | jpeg-xl-e9 |
|---|---|---|---|---|
| sk_camera_q85.pgm | **3.132** | 3.387 | 3.257 | 3.193 |
| sk_grass_q90.pgm | **6.245** | 6.403 | 6.278 | 6.312 |

## PPM 24 bpp (5 images)

| Codec | mean bpp | total bpp | vs JPEG-LS | wins | enc Mpix/s | dec Mpix/s | max mem enc / dec (MB) |
|---|---|---|---|---|---|---|---|
| ours | 7.109 | 7.431 | -36.6% | 0/5 | 0.58 | 1.41 | 8 / 6 |
| jpeg-ls | 11.066 | 11.178 | +0.0% | 0/5 | 5.91 | 7.16 | 8 / 8 |
| jpeg-xl-e7 | 6.856 | 7.186 | -39.0% | 1/5 | 0.50 | 2.97 | 48 / 17 |
| jpeg-xl-e9 | 6.817 | 7.148 | -39.3% | 4/5 | 0.03 | 2.96 | 71 / 17 |

bpp per image:

| Image | ours | jpeg-ls | jpeg-xl-e7 | jpeg-xl-e9 |
|---|---|---|---|---|
| sk_astronaut_q75_420.ppm | 5.852 | 9.844 | 5.734 | **5.683** |
| sk_chelsea_q80_420.ppm | 6.014 | 11.359 | 5.764 | **5.722** |
| sk_coffee_q90_420.ppm | 8.155 | 12.493 | 7.882 | **7.797** |
| sk_motorcycle_left_q95_444.ppm | 9.643 | 12.751 | 9.461 | **9.426** |
| ws_u76c0g_bliznaca_srgb8_q85_420.ppm | 5.879 | 8.885 | **5.440** | 5.459 |

## All images (7)

bpp is not averaged across groups (8, 16 and 24 bpp images); use the relative column.

| Codec | mean bpp | total bpp | vs JPEG-LS | wins | enc Mpix/s | dec Mpix/s | max mem enc / dec (MB) |
|---|---|---|---|---|---|---|---|
| ours | - | - | -28.8% | 2/7 | 0.70 | 1.71 | 8 / 6 |
| jpeg-ls | - | - | +0.0% | 0/7 | 7.26 | 8.88 | 8 / 8 |
| jpeg-xl-e7 | - | - | -30.3% | 1/7 | 0.59 | 3.38 | 48 / 17 |
| jpeg-xl-e9 | - | - | -30.7% | 4/7 | 0.04 | 3.52 | 71 / 17 |
