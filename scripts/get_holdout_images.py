#!/usr/bin/env python3
"""Builds a separate ("hold-out") test set: images never used to tune the codec.

The codec was tuned on the course images, so its results there are optimistic.
This script gathers other public test images and writes them in the layout the
benchmark expects (imgs-pgm8/, imgs-ppm/, imgs-raw16/ under the output dir):
  - scikit-image sample images (bundled with the package, lossless PNG):
    camera, moon, coins, brick, grass, gravel, page, text, cell,
    microaneurysms, astronaut, chelsea, coffee, motorcycle_left, ihc, color
  - libjxl test data (github.com/libjxl/testdata): the 500x500 "wesaturate"
    photos, flower (2268x1512, grey and colour), and flower_small at 8 bits
    and at 10/12/14/16 bits (written as big-endian 16-bit RAW files).
Note: flower comes from the JPEG XL test set, so if anything it favours JPEG XL.

Needs: pip install scikit-image imageio numpy; git.
Usage: python3 scripts/get_holdout_images.py OUT_DIR
Then:  mkdir -p OUT_DIR/scripts && cp scripts/benchmark_sota.py OUT_DIR/scripts/ &&
       ln -s "$PWD/bin" OUT_DIR/bin &&
       python3 OUT_DIR/scripts/benchmark_sota.py --groups pgm8 ppm raw16 --tag holdout
"""
import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile

import imageio.v3 as iio
import numpy as np
import skimage

SKIMAGE = ["camera", "moon", "coins", "brick", "grass", "gravel", "page", "text", "cell", "microaneurysms",
           "astronaut", "chelsea", "coffee", "motorcycle_left", "ihc", "color"]


def write_pnm(path, a, maxval=255):
    if a.ndim == 3 and a.shape[2] == 4:
        a = a[:, :, :3]
    magic = b"P6" if a.ndim == 3 else b"P5"
    h, w = a.shape[:2]
    data = a.astype(">u2").tobytes() if maxval > 255 else a.astype(np.uint8).tobytes()
    with open(path, "wb") as f:
        f.write(magic + b"\n%d %d\n%d\n" % (w, h, maxval) + data)


def main():
    out = sys.argv[1]
    for d in ("imgs-pgm8", "imgs-ppm", "imgs-raw16"):
        os.makedirs(os.path.join(out, d), exist_ok=True)

    data = os.path.join(os.path.dirname(skimage.__file__), "data")
    for n in SKIMAGE:
        a = iio.imread(os.path.join(data, n + ".png"))
        if a.ndim == 3 and a.shape[2] == 4:
            a = a[:, :, :3]
        if a.ndim == 3 and (a[:, :, 0] == a[:, :, 1]).all() and (a[:, :, 1] == a[:, :, 2]).all():
            a = a[:, :, 0]  # grey image stored as RGB
        if a.ndim == 3:
            write_pnm(os.path.join(out, "imgs-ppm", f"sk_{n}.ppm"), a)
        else:
            write_pnm(os.path.join(out, "imgs-pgm8", f"sk_{n}.pgm"), a)

    with tempfile.TemporaryDirectory() as tmp:
        td = os.path.join(tmp, "testdata")
        subprocess.run(["git", "clone", "-q", "--depth", "1", "--filter=blob:none", "--sparse",
                        "https://github.com/libjxl/testdata.git", td], check=True)
        subprocess.run(["git", "-C", td, "sparse-checkout", "set", "--no-cone", "/external/wesaturate/500px",
                        "/jxl/flower/flower.pgm", "/jxl/flower/flower.pnm", "/jxl/flower/flower_small.g.depth*.pgm"],
                       check=True)
        for f in sorted(glob.glob(os.path.join(td, "external/wesaturate/500px/*srgb8.png")) +
                        [os.path.join(td, "external/wesaturate/500px/cvo9xd_keong_macan_grayscale.png")]):
            a = iio.imread(f)
            n = os.path.basename(f)[:-4]
            if a.ndim == 3:
                write_pnm(os.path.join(out, "imgs-ppm", f"ws_{n}.ppm"), a)
            else:
                write_pnm(os.path.join(out, "imgs-pgm8", f"ws_{n}.pgm"), a)
        shutil.copy(os.path.join(td, "jxl/flower/flower.pnm"), os.path.join(out, "imgs-ppm", "flower.ppm"))
        shutil.copy(os.path.join(td, "jxl/flower/flower.pgm"), os.path.join(out, "imgs-pgm8", "flower.pgm"))
        shutil.copy(os.path.join(td, "jxl/flower/flower_small.g.depth8.pgm"),
                    os.path.join(out, "imgs-pgm8", "flower_small_d8.pgm"))
        for dep in (10, 12, 14, 16):
            with open(os.path.join(td, f"jxl/flower/flower_small.g.depth{dep}.pgm"), "rb") as f:
                p = f.read()
            t = re.sub(rb"#[^\n]*\n", b" ", p[:100]).split()
            w, h = int(t[1]), int(t[2])
            with open(os.path.join(out, "imgs-raw16", f"flower_small_depth{dep}-u16be-1x{h}x{w}.raw"), "wb") as f:
                f.write(p[len(p) - 2 * w * h:])
    print("hold-out images written to", out)


if __name__ == "__main__":
    main()
