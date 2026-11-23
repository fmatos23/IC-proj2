#!/usr/bin/env python3
"""Generates small images covering the special cases of the lossless codec.

The course images are all binary PGM/PPM with plain headers and 16-bit
big-endian RAW files; these cover what they do not: ASCII PNM, comments,
odd maxval values, tiny sizes, little-endian and 8-bit RAW, trailing bytes,
invalid samples, non-image files...

Usage: make_special_images.py output_dir
Writes the files and output_dir/manifest.txt, one line per file:
    file_name<TAB>encoder options (RAW parameters, may be empty)
"""
import os
import random
import struct
import sys


def pnm(magic, w, h, maxval, samples, comment=None, trailer=b""):
    hdr = magic.encode() + b"\n"
    if comment:
        hdr += b"# " + comment.encode() + b"\n"
    hdr += b"%d %d\n%d\n" % (w, h, maxval)
    if magic in ("P2", "P3"):
        body = b"\n".join(b" ".join(str(v).encode() for v in samples[i:i + 12])
                          for i in range(0, len(samples), 12)) + b"\n"
    elif maxval > 255:
        body = struct.pack(">%dH" % len(samples), *samples)
    else:
        body = bytes(samples)
    return hdr + body + trailer


def smooth(w, h, c, maxval, seed):
    """Smooth gradient with some noise, values in [0, maxval]."""
    rnd = random.Random(seed)
    out = []
    for r in range(h):
        for x in range(w):
            for ch in range(c):
                v = (r * 3 + x * 2 + ch * 40) * maxval // (3 * h + 2 * w + 80 + 1)
                v += rnd.randint(-2, 2) * max(1, maxval // 255)
                out.append(min(maxval, max(0, v)))
    return out


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    files = []  # (name, data, options)

    def add(name, data, opts=""):
        files.append((name, data, opts))

    rnd = random.Random(1)
    add("p5_basic.pgm", pnm("P5", 37, 23, 255, smooth(37, 23, 1, 255, 1)))
    add("p5_comment.pgm", pnm("P5", 37, 23, 255, smooth(37, 23, 1, 255, 2), comment="created by hand"))
    add("p6_comment_trailing_newline.ppm", pnm("P6", 20, 15, 255, smooth(20, 15, 3, 255, 3),
                                               comment="x", trailer=b"\n"))
    add("p5_16bit.pgm", pnm("P5", 31, 17, 65535, smooth(31, 17, 1, 65535, 4)))
    add("p5_maxval1000.pgm", pnm("P5", 31, 17, 1000, smooth(31, 17, 1, 1000, 5)))
    add("p5_maxval1_binary.pgm", pnm("P5", 40, 10, 1, [rnd.randint(0, 1) for _ in range(400)]))
    add("p5_maxval100.pgm", pnm("P5", 16, 16, 100, smooth(16, 16, 1, 100, 6)))
    add("p6_16bit.ppm", pnm("P6", 13, 11, 65535, smooth(13, 11, 3, 65535, 7)))
    add("p6_maxval4095.ppm", pnm("P6", 13, 11, 4095, smooth(13, 11, 3, 4095, 8)))
    add("p5_1x1.pgm", pnm("P5", 1, 1, 255, [77]))
    add("p6_1x1.ppm", pnm("P6", 1, 1, 255, [1, 2, 3]))
    add("p5_row_1x50.pgm", pnm("P5", 50, 1, 255, smooth(50, 1, 1, 255, 9)))
    add("p5_col_50x1.pgm", pnm("P5", 1, 50, 255, smooth(1, 50, 1, 255, 10)))
    add("p5_constant.pgm", pnm("P5", 64, 64, 255, [200] * 4096))
    add("p5_extremes.pgm", pnm("P5", 32, 32, 255, [rnd.choice((0, 255)) for _ in range(1024)]))
    add("p5_noise.pgm", pnm("P5", 64, 64, 255, [rnd.randint(0, 255) for _ in range(4096)]))
    add("p6_noise_16bit.ppm", pnm("P6", 16, 16, 65535, [rnd.randint(0, 65535) for _ in range(768)]))
    # repeated patterns (synthetic image: exercises the match model)
    # (a 17x17 tile of random colours: more than 256 colours, so not the palette mode)
    tile = [rnd.randint(0, 255) for _ in range(17 * 17 * 3)]
    rep = []
    for r in range(68):
        for x in range(102):
            k = ((r % 17) * 17 + (x % 17)) * 3
            rep += tile[k:k + 3] if (r // 17 + x // 17) % 3 else [250, 250, 250]
    add("p6_repeated_pattern.ppm", pnm("P6", 102, 68, 255, rep))
    # values on a grid (value scaling): 12-bit data in the top bits of 16-bit
    # samples, a different step and offset per channel, and a grey image
    s12 = smooth(40, 30, 3, 4095, 16)
    add("p6_16bit_shifted12.ppm", pnm("P6", 40, 30, 65535, [v << 4 for v in s12]))
    sg = smooth(40, 30, 3, 900, 17)
    add("p6_16bit_step_offset.ppm",
        pnm("P6", 40, 30, 65535, [v * (16, 7, 1)[i % 3] + (3, 5, 0)[i % 3] for i, v in enumerate(sg)]))
    add("p5_16bit_step10_offset3.pgm", pnm("P5", 40, 30, 65535, [v * 10 + 3 for v in smooth(40, 30, 1, 6000, 18)]))
    # cases that must go through the generic (bytes) mode
    add("p2_ascii.pgm", pnm("P2", 9, 7, 255, smooth(9, 7, 1, 255, 11), comment="ascii"))
    add("p3_ascii.ppm", pnm("P3", 9, 7, 255, smooth(9, 7, 3, 255, 12)))
    add("p5_sample_above_maxval.pgm", pnm("P5", 8, 8, 100, [rnd.randint(0, 255) for _ in range(64)]))
    add("p5_truncated.pgm", pnm("P5", 20, 20, 255, smooth(20, 20, 1, 255, 13))[:-50])
    add("not_an_image.txt", b"This is not an image.\n" * 30)
    add("empty.pgm", b"")
    # RAW files
    s8 = smooth(45, 30, 1, 255, 14)
    add("raw8-1x30x45.raw", bytes(s8), "-rows 30 -cols 45")
    s16 = smooth(45, 30, 1, 65535, 15)
    add("raw16be-1x30x45.raw", struct.pack(">%dH" % len(s16), *s16), "-rows 30 -cols 45 -bpp 16 -be")
    add("raw16le-1x30x45.raw", struct.pack("<%dH" % len(s16), *s16), "-rows 30 -cols 45 -bpp 16 -le")
    add("raw16be_trailing-1x30x45.raw", struct.pack(">%dH" % len(s16), *s16) + b"extra bytes",
        "-rows 30 -cols 45 -bpp 16 -be")
    add("raw16_12bit_values-1x30x45.raw",
        struct.pack(">%dH" % len(s16), *[v >> 4 for v in s16]), "-rows 30 -cols 45 -bpp 16")
    add("raw8_1x1-1x1x1.raw", b"\x05", "-rows 1 -cols 1")

    with open(os.path.join(out, "manifest.txt"), "w") as m:
        for name, data, opts in files:
            with open(os.path.join(out, name), "wb") as f:
                f.write(data)
            m.write(f"{name}\t{opts}\n")
    print(f"{len(files)} files written to {out}")


if __name__ == "__main__":
    main()
