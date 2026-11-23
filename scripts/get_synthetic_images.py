#!/usr/bin/env python3
"""Builds a set of synthetic (non-photographic) test images: charts, text,
screenshot-like layouts, colour bars and gradients.

The set is split in two so that the evaluation stays honest:
  dev/   images used while designing the synthetic-image tools of the codec
  test/  images used only to measure the result
Each part has the benchmark layout (imgs-pgm8/, imgs-ppm/).

Sources: images drawn here with matplotlib and Pillow (deterministic), the
scikit-image samples logo/chessboard/text/page, and the libjxl test data
(github.com/libjxl/testdata: wide-gamut test charts, colourful chessboards,
ellipses).

Needs: pip install matplotlib pillow scikit-image imageio numpy; git.
Usage: python3 scripts/get_synthetic_images.py OUT_DIR
"""
import io
import os
import subprocess
import sys
import tempfile

import imageio.v3 as iio
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
import skimage  # noqa: E402
from PIL import Image, ImageDraw, ImageFont  # noqa: E402


def write_pnm(path, a):
    a = np.asarray(a)
    if a.ndim == 3 and a.shape[2] == 4:
        a = a[:, :, :3]
    if a.ndim == 3 and (a[:, :, 0] == a[:, :, 1]).all() and (a[:, :, 1] == a[:, :, 2]).all():
        a = a[:, :, 0]
    a = a.astype(np.uint8)
    magic, ext = (b"P6", "ppm") if a.ndim == 3 else (b"P5", "pgm")
    h, w = a.shape[:2]
    path = f"{path}.{ext}"
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(magic + b"\n%d %d\n255\n" % (w, h) + a.tobytes())


def fig_to_array(fig, dpi=100):
    buf = io.BytesIO()
    fig.savefig(buf, format="png", dpi=dpi)
    plt.close(fig)
    return iio.imread(buf.getvalue())


def charts(rng):
    out = {}
    x = np.linspace(0, 10, 400)
    fig, ax = plt.subplots(figsize=(8, 5))
    for k in range(4):
        ax.plot(x, np.sin(x * (k + 1) / 2) + k, label=f"series {k}")
    ax.set_title("Line chart")
    ax.legend()
    ax.grid(True)
    out["line_chart"] = fig_to_array(fig)

    fig, ax = plt.subplots(figsize=(8, 5))
    ax.bar([f"C{k}" for k in range(12)], rng.integers(5, 50, 12), color=plt.cm.tab20.colors[:12])
    ax.set_title("Bar chart")
    out["bar_chart"] = fig_to_array(fig)

    fig, ax = plt.subplots(figsize=(8, 6))
    ax.scatter(rng.normal(size=600), rng.normal(size=600), c=rng.random(600), cmap="viridis", s=12)
    ax.set_title("Scatter plot")
    out["scatter"] = fig_to_array(fig)

    fig, ax = plt.subplots(figsize=(7, 6))
    im = ax.imshow(np.add.outer(np.sin(np.linspace(0, 6, 300)), np.cos(np.linspace(0, 4, 300))), cmap="magma")
    fig.colorbar(im)
    ax.set_title("Heat map")
    out["heatmap"] = fig_to_array(fig)

    fig, ax = plt.subplots(figsize=(6, 6))
    ax.pie(rng.integers(1, 10, 7), labels=[f"slice {k}" for k in range(7)], autopct="%1.0f%%")
    out["pie_chart"] = fig_to_array(fig)

    fig, axs = plt.subplots(2, 2, figsize=(9, 7))
    for k, a in enumerate(axs.flat):
        a.hist(rng.normal(k, 1 + k / 3, 2000), bins=40, color=plt.cm.Set2.colors[k])
        a.set_title(f"histogram {k}")
    fig.tight_layout()
    out["histograms"] = fig_to_array(fig)
    return out


LOREM = ("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore "
         "et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut "
         "aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum.")


def font(size):
    for f in ("DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(f, size)
        except OSError:
            pass
    return ImageFont.load_default()


def documents(rng):
    out = {}
    # text document (grey, anti-aliased)
    img = Image.new("L", (850, 1100), 255)
    d = ImageDraw.Draw(img)
    d.text((60, 40), "Report title", font=font(36), fill=0)
    y = 110
    for p in range(9):
        words = LOREM.split()
        line = ""
        for w in words:
            if d.textlength(line + w, font=font(16)) > 730:
                d.text((60, y), line, font=font(16), fill=20)
                y += 22
                line = ""
            line += w + " "
        d.text((60, y), line, font=font(16), fill=20)
        y += 40
    out["text_document"] = np.array(img)

    # screenshot-like user interface (colour)
    img = Image.new("RGB", (1024, 700), (240, 242, 245))
    d = ImageDraw.Draw(img)
    d.rectangle((0, 0, 1024, 48), fill=(33, 37, 41))
    d.text((16, 12), "My Application", font=font(20), fill=(255, 255, 255))
    d.rectangle((0, 48, 200, 700), fill=(52, 58, 64))
    for k, item in enumerate(["Dashboard", "Projects", "Messages", "Calendar", "Settings"]):
        d.text((20, 70 + 40 * k), item, font=font(16), fill=(200, 200, 200))
    for k in range(6):
        x0, y0 = 220 + (k % 3) * 265, 70 + (k // 3) * 300
        d.rounded_rectangle((x0, y0, x0 + 245, y0 + 280), 10, fill=(255, 255, 255), outline=(210, 210, 210))
        d.text((x0 + 14, y0 + 12), f"Card {k + 1}", font=font(18), fill=(30, 30, 30))
        for j in range(5):
            d.text((x0 + 14, y0 + 50 + 22 * j), LOREM[j * 30:j * 30 + 28], font=font(12), fill=(90, 90, 90))
        col = tuple(int(c) for c in rng.integers(40, 220, 3))
        d.rectangle((x0 + 14, y0 + 180, x0 + 14 + int(rng.integers(60, 210)), y0 + 200), fill=col)
        d.rounded_rectangle((x0 + 14, y0 + 225, x0 + 110, y0 + 260), 6, fill=(13, 110, 253))
        d.text((x0 + 30, y0 + 233), "Open", font=font(14), fill=(255, 255, 255))
    out["ui_screenshot"] = np.array(img)

    # spreadsheet-like table
    img = Image.new("RGB", (900, 600), (255, 255, 255))
    d = ImageDraw.Draw(img)
    for r in range(25):
        for c in range(8):
            x0, y0 = 10 + c * 110, 10 + r * 23
            fill = (220, 230, 241) if r == 0 else ((248, 248, 248) if r % 2 else (255, 255, 255))
            d.rectangle((x0, y0, x0 + 110, y0 + 23), fill=fill, outline=(200, 200, 200))
            txt = f"Col {c}" if r == 0 else f"{rng.integers(0, 100000) / 100:.2f}"
            d.text((x0 + 6, y0 + 4), txt, font=font(13), fill=(0, 0, 0))
    out["spreadsheet"] = np.array(img)

    # web-page-like layout with a gradient banner and coloured text
    img = Image.new("RGB", (1000, 800), (255, 255, 255))
    banner = np.zeros((180, 1000, 3), np.uint8)
    banner[..., 0] = np.linspace(20, 200, 1000)[None, :]
    banner[..., 1] = np.linspace(60, 120, 180)[:, None]
    banner[..., 2] = 180
    img.paste(Image.fromarray(banner), (0, 0))
    d = ImageDraw.Draw(img)
    d.text((40, 60), "Welcome to the page", font=font(40), fill=(255, 255, 255))
    y = 210
    for k in range(14):
        d.text((40, y), LOREM[:110], font=font(15), fill=(40, 40, 40) if k % 4 else (180, 30, 30))
        y += 26
    d.rectangle((40, 600, 960, 760), fill=(245, 245, 220), outline=(180, 170, 120))
    d.text((60, 620), "Note: " + LOREM[:80], font=font(15), fill=(90, 80, 20))
    out["web_page"] = np.array(img)
    return out


def main():
    out = sys.argv[1]
    rng = np.random.default_rng(12345)
    imgs = {}
    imgs.update(charts(rng))
    imgs.update(documents(rng))
    data = os.path.join(os.path.dirname(skimage.__file__), "data")
    for n in ["logo", "chessboard_RGB", "text", "page"]:
        imgs["sk_" + n] = iio.imread(os.path.join(data, n + ".png"))
    with tempfile.TemporaryDirectory() as tmp:
        td = os.path.join(tmp, "testdata")
        subprocess.run(["git", "clone", "-q", "--depth", "1", "--filter=blob:none", "--sparse",
                        "https://github.com/libjxl/testdata.git", td], check=True)
        subprocess.run(["git", "-C", td, "sparse-checkout", "set", "--no-cone", "/external/wide-gamut-tests",
                        "/jxl/chessboard", "/dots"], check=True)
        for f in ["external/wide-gamut-tests/P3-sRGB-color-bars.png", "external/wide-gamut-tests/P3-sRGB-color-ring.png",
                  "external/wide-gamut-tests/R2020-sRGB-color-bars.png", "external/wide-gamut-tests/R2020-sRGB-color-ring.png",
                  "external/wide-gamut-tests/rgb-to-gbr-test.png", "jxl/chessboard/colorful_chessboards.png",
                  "dots/ellipses.png"]:
            a = iio.imread(os.path.join(td, f))
            if a.dtype != np.uint8:
                a = (a >> 8).astype(np.uint8)
            imgs["jxl_" + os.path.splitext(os.path.basename(f))[0]] = a

    # development images (used while designing) and test images (only measured)
    dev = {"line_chart", "bar_chart", "text_document", "ui_screenshot", "jxl_P3-sRGB-color-bars", "sk_logo"}
    for name, a in imgs.items():
        part = "dev" if name in dev else "test"
        sub = "imgs-ppm" if (np.asarray(a).ndim == 3 and not (
            (a[:, :, 0] == a[:, :, 1]).all() and (a[:, :, 1] == a[:, :, 2]).all())) else "imgs-pgm8"
        write_pnm(os.path.join(out, part, sub, name), a)
    print("synthetic images written to", out)


if __name__ == "__main__":
    main()
