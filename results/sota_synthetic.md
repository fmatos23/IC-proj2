# Lossless state-of-the-art benchmark

Machine: vm, Intel(R) Xeon(R) Processor @ 2.10GHz, 2 CPUs, Linux 6.18.44-fc-v726  
Times: minimum of 3 run(s) (JPEG-XL -e 9: 1 run), single thread, wall clock, including file I/O.  
Versions: cjxl: cjxl v0.7.0 [AVX3_SPR,AVX3,AVX2,SSE4,SSSE3,SSE2]; optipng: OptiPNG version 0.7.8; zstd: *** Zstandard CLI (64-bit) v1.5.5, by Yann Collet ***; xz: xz (XZ Utils) 5.4.5; bzip2: bzip2, a block-sorting file compressor.  Version 1.0.8, 13-Jul-2019.
Rows of the reference codecs taken from an earlier run on the same images and machine.  

bpp = compressed bits / number of pixels (a PPM pixel has 3 samples: raw size 24 bpp).  
mean bpp: every image weighs the same; total bpp: all bits / all pixels.  
vs JPEG-LS: geometric mean of the per-image size ratio to JPEG-LS (negative = smaller).  
Mpix/s: megapixels per second (all pixels / total time).

## PGM 8 bpp (3 images)

| Codec | mean bpp | total bpp | vs JPEG-LS | wins | enc Mpix/s | dec Mpix/s | max mem enc / dec (MB) |
|---|---|---|---|---|---|---|---|
| ours | 2.669 | 3.154 | -57.2% | 2/3 | 0.76 | 3.36 | 7 / 5 |
| jpeg-ls | 3.033 | 3.490 | +0.0% | 0/3 | 10.83 | 10.15 | 4 / 5 |
| jpeg-xl-e7 | 2.767 | 3.266 | -50.3% | 0/3 | 1.28 | 3.75 | 16 / 10 |
| jpeg-xl-e9 | 2.727 | 3.222 | -57.4% | 0/3 | 0.09 | 4.19 | 18 / 10 |
| png-optipng | 3.032 | 3.581 | -52.4% | 0/3 | 2.93 | 11.38 | 3 / 3 |
| xz-9 | 3.273 | 3.870 | -53.3% | 0/3 | 1.29 | 9.98 | 62 / 2 |
| bzip2-9 | 3.405 | 4.026 | -56.5% | 1/3 | 7.26 | 10.41 | 2 / 2 |

bpp per image:

| Image | ours | jpeg-ls | jpeg-xl-e7 | jpeg-xl-e9 | png-optipng | xz-9 | bzip2-9 |
|---|---|---|---|---|---|---|---|
| sk_chessboard_RGB.pgm | 0.050 | 0.557 | 0.074 | 0.048 | 0.054 | 0.043 | **0.032** |
| sk_page.pgm | **3.955** | 4.315 | 4.113 | 4.062 | 4.635 | 4.917 | 5.205 |
| sk_text.pgm | **4.003** | 4.227 | 4.115 | 4.070 | 4.408 | 4.859 | 4.977 |

## PPM 24 bpp (12 images)

| Codec | mean bpp | total bpp | vs JPEG-LS | wins | enc Mpix/s | dec Mpix/s | max mem enc / dec (MB) |
|---|---|---|---|---|---|---|---|
| ours | 1.458 | 1.708 | -66.5% | 1/12 | 1.40 | 2.28 | 24 / 24 |
| jpeg-ls | 3.619 | 4.527 | +0.0% | 0/12 | 13.19 | 19.57 | 14 / 14 |
| jpeg-xl-e7 | 1.436 | 1.683 | -64.6% | 0/12 | 1.06 | 6.96 | 144 / 39 |
| jpeg-xl-e9 | 1.187 | 1.387 | -73.2% | 10/12 | 0.09 | 6.09 | 205 / 39 |
| png-optipng | 2.084 | 2.469 | -45.0% | 0/12 | 0.43 | 27.24 | 6 / 6 |
| xz-9 | 1.772 | 2.138 | -62.8% | 1/12 | 1.18 | 39.16 | 87 / 5 |
| bzip2-9 | 1.776 | 2.073 | -62.5% | 0/12 | 4.21 | 17.70 | 9 / 5 |

bpp per image:

| Image | ours | jpeg-ls | jpeg-xl-e7 | jpeg-xl-e9 | png-optipng | xz-9 | bzip2-9 |
|---|---|---|---|---|---|---|---|
| heatmap.ppm | 1.165 | 2.080 | 1.043 | **0.893** | 1.943 | 1.359 | 1.075 |
| histograms.ppm | 0.223 | 0.764 | 0.172 | **0.135** | 0.414 | 0.178 | 0.181 |
| jxl_P3-sRGB-color-ring.ppm | 3.553 | 4.537 | 3.484 | **2.995** | 5.066 | 4.347 | 3.241 |
| jxl_R2020-sRGB-color-bars.ppm | 3.673 | 4.688 | 3.417 | **3.182** | 4.767 | 5.578 | 8.121 |
| jxl_R2020-sRGB-color-ring.ppm | 3.601 | 4.628 | 3.424 | **2.867** | 5.138 | 4.202 | 3.140 |
| jxl_colorful_chessboards.ppm | 0.618 | 12.617 | 1.078 | **0.425** | 0.902 | 0.529 | 0.537 |
| jxl_ellipses.ppm | 1.329 | 1.941 | 1.416 | 1.158 | 1.503 | **1.106** | 1.343 |
| jxl_rgb-to-gbr-test.ppm | 1.565 | 4.512 | 1.383 | **1.221** | 2.391 | 2.149 | 1.691 |
| pie_chart.ppm | 0.435 | 0.540 | 0.408 | **0.307** | 0.513 | 0.389 | 0.381 |
| scatter.ppm | 0.986 | 1.197 | 0.869 | **0.694** | 1.069 | 0.921 | 0.867 |
| spreadsheet.ppm | **0.133** | 3.326 | 0.330 | 0.219 | 0.731 | 0.304 | 0.231 |
| web_page.ppm | 0.212 | 2.593 | 0.204 | **0.146** | 0.568 | 0.204 | 0.497 |

## All images (15)

bpp is not averaged across groups (8, 16 and 24 bpp images); use the relative column.

| Codec | mean bpp | total bpp | vs JPEG-LS | wins | enc Mpix/s | dec Mpix/s | max mem enc / dec (MB) |
|---|---|---|---|---|---|---|---|
| ours | - | - | -64.9% | 3/15 | 1.37 | 2.29 | 24 / 24 |
| jpeg-ls | - | - | +0.0% | 0/15 | 13.13 | 19.16 | 14 / 14 |
| jpeg-xl-e7 | - | - | -62.1% | 0/15 | 1.07 | 6.82 | 144 / 39 |
| jpeg-xl-e9 | - | - | -70.6% | 10/15 | 0.09 | 6.03 | 205 / 39 |
| png-optipng | - | - | -46.6% | 0/15 | 0.44 | 26.40 | 6 / 6 |
| xz-9 | - | - | -61.0% | 1/15 | 1.19 | 36.70 | 87 / 5 |
| bzip2-9 | - | - | -61.4% | 1/15 | 4.25 | 17.42 | 9 / 5 |
