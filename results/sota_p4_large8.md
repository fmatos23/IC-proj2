# Lossless state-of-the-art benchmark

Machine: vm, Intel(R) Xeon(R) Processor @ 2.10GHz, 2 CPUs, Linux 6.18.44-fc-v726  
Times: minimum of 1 run(s) (JPEG-XL -e 9: 1 run), single thread, wall clock, including file I/O.  
Versions: cjxl: cjxl v0.7.0 [AVX3_SPR,AVX3,AVX2,SSE4,SSSE3,SSE2]; optipng: OptiPNG version 0.7.8; zstd: *** Zstandard CLI (64-bit) v1.5.5, by Yann Collet ***; xz: xz (XZ Utils) 5.4.5; bzip2: bzip2, a block-sorting file compressor.  Version 1.0.8, 13-Jul-2019.
Rows of codecs not run now taken from sota_p4_large8.csv.  

bpp = compressed bits / number of pixels (a PPM pixel has 3 samples: raw size 24 bpp).  
mean bpp: every image weighs the same; total bpp: all bits / all pixels.  
vs JPEG-LS: geometric mean of the per-image size ratio to JPEG-LS (negative = smaller).  
Mpix/s: megapixels per second (all pixels / total time).

## PPM 24 bpp (3 images)

| Codec | mean bpp | total bpp | vs JPEG-LS | wins | enc Mpix/s | dec Mpix/s | max mem enc / dec (MB) |
|---|---|---|---|---|---|---|---|
| ours | 7.102 | 7.243 | -21.6% | 0/3 | 1.16 | 1.37 | 32 / 24 |
| ours-fast | 7.308 | 7.576 | -19.3% | 0/3 | 1.54 | 2.16 | 33 / 25 |
| ours-mt | 7.103 | 7.243 | -21.6% | 0/3 | 1.82 | 2.58 | 41 / 26 |
| jpeg-ls | 9.164 | 9.717 | +0.0% | 0/3 | 6.80 | 8.13 | 49 / 47 |
| jpeg-xl-e7 | 7.054 | 6.726 | -22.3% | 0/3 | 0.39 | 3.49 | 444 / 102 |
| jpeg-xl-e9 | 6.788 | 6.587 | -25.2% | 3/3 | 0.05 | 3.17 | 760 / 101 |
| png-optipng | 9.779 | 9.968 | +7.1% | 0/3 | 0.42 | 23.47 | 17 / 17 |
| zstd-19 | 10.870 | 11.507 | +20.9% | 0/3 | 0.86 | 90.23 | 106 / 12 |
| xz-9 | 9.898 | 10.678 | +9.6% | 0/3 | 0.73 | 14.44 | 197 / 16 |
| bzip2-9 | 9.073 | 9.635 | +1.0% | 0/3 | 5.70 | 6.29 | 8 / 5 |

bpp per image:

| Image | ours | ours-fast | ours-mt | jpeg-ls | jpeg-xl-e7 | jpeg-xl-e9 | png-optipng | zstd-19 | xz-9 | bzip2-9 |
|---|---|---|---|---|---|---|---|---|---|---|
| Bretagne1.ppm | 8.382 | 8.509 | 8.391 | 11.155 | 8.799 | **8.327** | 12.061 | 11.762 | 10.927 | 9.733 |
| Bretagne2.ppm | 7.271 | 7.628 | 7.270 | 9.825 | 6.663 | **6.548** | 10.005 | 11.631 | 10.830 | 9.744 |
| Cevennes2.ppm | 5.654 | 5.786 | 5.648 | 6.512 | 5.701 | **5.488** | 7.271 | 9.216 | 7.938 | 7.743 |
