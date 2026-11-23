# Lossless state-of-the-art benchmark

Machine: vm, Intel(R) Xeon(R) Processor @ 2.10GHz, 2 CPUs, Linux 6.18.44-fc-v726  
Times: minimum of 1 run(s) (JPEG-XL -e 9: 1 run), single thread, wall clock, including file I/O.  
Versions: cjxl: cjxl v0.7.0 [AVX3_SPR,AVX3,AVX2,SSE4,SSSE3,SSE2]; optipng: OptiPNG version 0.7.8; zstd: *** Zstandard CLI (64-bit) v1.5.5, by Yann Collet ***; xz: xz (XZ Utils) 5.4.5; bzip2: bzip2, a block-sorting file compressor.  Version 1.0.8, 13-Jul-2019.
Rows of codecs not run now taken from sota_jpg_test.csv.  

bpp = compressed bits / number of pixels (a PPM pixel has 3 samples: raw size 24 bpp).  
mean bpp: every image weighs the same; total bpp: all bits / all pixels.  
vs JPEG-LS: geometric mean of the per-image size ratio to JPEG-LS (negative = smaller).  
Mpix/s: megapixels per second (all pixels / total time).

## PGM 8 bpp (1 images)

| Codec | mean bpp | total bpp | vs JPEG-LS | wins | enc Mpix/s | dec Mpix/s | max mem enc / dec (MB) |
|---|---|---|---|---|---|---|---|
| ours | 2.474 | 2.474 | -14.5% | 1/1 | 4.06 | 4.29 | 11 / 9 |
| jpeg-ls | 2.894 | 2.894 | +0.0% | 0/1 | 23.26 | 28.05 | 15 / 14 |
| jpeg-xl-e7 | 2.669 | 2.669 | -7.8% | 0/1 | 1.16 | 9.30 | 174 / 56 |
| jpeg-xl-e9 | 2.666 | 2.666 | -7.9% | 0/1 | 0.10 | 9.35 | 241 / 56 |

bpp per image:

| Image | ours | jpeg-ls | jpeg-xl-e7 | jpeg-xl-e9 |
|---|---|---|---|---|
| flower_q85_gray.pgm | **2.474** | 2.894 | 2.669 | 2.666 |

## PPM 24 bpp (7 images)

| Codec | mean bpp | total bpp | vs JPEG-LS | wins | enc Mpix/s | dec Mpix/s | max mem enc / dec (MB) |
|---|---|---|---|---|---|---|---|
| ours | 6.699 | 6.603 | -29.7% | 0/7 | 1.14 | 1.44 | 32 / 24 |
| jpeg-ls | 9.624 | 9.445 | +0.0% | 0/7 | 6.75 | 7.31 | 49 / 47 |
| jpeg-xl-e7 | 6.610 | 6.305 | -30.8% | 0/7 | 0.38 | 3.39 | 444 / 102 |
| jpeg-xl-e9 | 6.437 | 6.204 | -32.5% | 7/7 | 0.04 | 2.95 | 760 / 101 |

bpp per image:

| Image | ours | jpeg-ls | jpeg-xl-e7 | jpeg-xl-e9 |
|---|---|---|---|---|
| Bretagne1.ppm | 8.382 | 11.155 | 8.799 | **8.327** |
| Bretagne2.ppm | 7.271 | 9.825 | 6.663 | **6.548** |
| Cevennes2.ppm | 5.654 | 6.512 | 5.701 | **5.488** |
| bicycles.ppm | 6.816 | 12.483 | 6.644 | **6.451** |
| flower_cropped.ppm | 6.688 | 9.559 | 6.749 | **6.660** |
| flower_q85_420.ppm | 5.906 | 8.892 | 5.736 | **5.681** |
| flower_q85_444.ppm | 6.176 | 8.942 | 5.975 | **5.905** |

## All images (8)

bpp is not averaged across groups (8, 16 and 24 bpp images); use the relative column.

| Codec | mean bpp | total bpp | vs JPEG-LS | wins | enc Mpix/s | dec Mpix/s | max mem enc / dec (MB) |
|---|---|---|---|---|---|---|---|
| ours | - | - | -28.0% | 1/8 | 1.33 | 1.66 | 32 / 24 |
| jpeg-ls | - | - | +0.0% | 0/8 | 7.83 | 8.54 | 49 / 47 |
| jpeg-xl-e7 | - | - | -28.3% | 0/8 | 0.44 | 3.87 | 444 / 102 |
| jpeg-xl-e9 | - | - | -29.8% | 7/8 | 0.04 | 3.40 | 760 / 101 |
