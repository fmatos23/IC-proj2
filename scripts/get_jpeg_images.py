#!/usr/bin/env python3
"""Images decoded from JPEG files (point 5: the codec was weakest on them).

  dev/   for design: hold-out photographs (scripts/get_holdout_images.py)
         compressed with JPEG (quality 75-95, 4:2:0 or 4:4:4) and decoded again.
  test/  for measuring only: real JPEG files from the libjxl test data
         (github.com/libjxl/testdata), decoded with Pillow, plus the three
         8-bit photographs of scripts/get_more_images.py (large8/).

Usage: python3 scripts/get_jpeg_images.py HOLDOUT_DIR MORE_DIR OUT_DIR
Needs: pip install pillow; git.
"""
import io
import os
import shutil
import subprocess
import sys
import tempfile

from PIL import Image

DEV_PPM = [("sk_chelsea", 80, 2), ("sk_coffee", 90, 2), ("sk_astronaut", 75, 2),
           ("sk_motorcycle_left", 95, 0), ("ws_u76c0g_bliznaca_srgb8", 85, 2)]
DEV_PGM = [("sk_camera", 85), ("sk_grass", 90)]
TEST = [("flower/flower.png.im_q85_420.jpg", "flower_q85_420.ppm"),
        ("flower/flower.png.im_q85_444.jpg", "flower_q85_444.ppm"),
        ("flower/flower_cropped.jpg", "flower_cropped.ppm"),
        ("jpeg_reconstruction/bicycles_restarts.jpg", "bicycles.ppm"),
        ("flower/flower.png.im_q85_gray.jpg", "flower_q85_gray.pgm")]


def roundtrip(src, dst, q, sub, gray):
    im = Image.open(src).convert("L" if gray else "RGB")
    b = io.BytesIO()
    im.save(b, "JPEG", quality=q, subsampling=sub)
    b.seek(0)
    Image.open(b).save(dst)


def main():
    holdout, more, out = sys.argv[1:4]
    for d in ("dev/imgs-ppm", "dev/imgs-pgm8", "test/imgs-ppm", "test/imgs-pgm8"):
        os.makedirs(os.path.join(out, d), exist_ok=True)
    for n, q, s in DEV_PPM:
        roundtrip(os.path.join(holdout, "imgs-ppm", n + ".ppm"),
                  os.path.join(out, "dev/imgs-ppm", f"{n}_q{q}_{['444', '422', '420'][s]}.ppm"), q, s, False)
    for n, q in DEV_PGM:
        roundtrip(os.path.join(holdout, "imgs-pgm8", n + ".pgm"),
                  os.path.join(out, "dev/imgs-pgm8", f"{n}_q{q}.pgm"), q, 0, True)
    with tempfile.TemporaryDirectory() as tmp:
        td = os.path.join(tmp, "testdata")
        subprocess.run(["git", "clone", "-q", "--depth", "1", "--filter=blob:none", "--sparse",
                        "https://github.com/libjxl/testdata.git", td], check=True)
        subprocess.run(["git", "-C", td, "sparse-checkout", "set", "--no-cone"] +
                       ["/jxl/" + f for f, _ in TEST], check=True)
        for f, o in TEST:
            im = Image.open(os.path.join(td, "jxl", f))
            im = im.convert("L" if o.endswith(".pgm") else "RGB")
            im.save(os.path.join(out, "test", "imgs-pgm8" if o.endswith(".pgm") else "imgs-ppm", o))
    for f in os.listdir(os.path.join(more, "large8/imgs-ppm")):
        shutil.copy(os.path.join(more, "large8/imgs-ppm", f), os.path.join(out, "test/imgs-ppm", f))
    print("images written to", out)


if __name__ == "__main__":
    main()
