#!/usr/bin/env python3
"""Benchmark of state-of-the-art lossless codecs on the test images (Part II.5).

For every image and codec it measures:
  - the compressed size, as bits per pixel (bpp = 8 * bytes / (rows * cols));
  - encoding and decoding wall-clock time (minimum over --repeat runs);
  - peak memory (max RSS, from /usr/bin/time);
and checks that decoding gives back exactly the original samples.

Codecs:
  image specific : JPEG-LS (CharLS, via bin/charls_cli), JPEG-XL lossless
                   (cjxl -d 0, several efforts), PNG (optipng -o2)
  general purpose: zstd -19, xz -9, bzip2 -9 (applied to the original file)

All codecs run single-threaded, so they are comparable with our own codec.

Image codecs need a PNM file: 16-bit RAW images are converted to a 16-bit
PGM first (same samples, conversion not timed); gzipped PPMs (kodak/) are
decompressed first. General-purpose compressors get the original file.

Usage (from the repository root, after `make bin/charls_cli`):
  python3 scripts/benchmark_sota.py                 # everything
  python3 scripts/benchmark_sota.py --skip-slow     # without JPEG-XL -e 9
  python3 scripts/benchmark_sota.py --repeat 3 --groups pgm8 ppm
Results go to results/sota_<host>.csv (one row per image x codec) and
results/sota_<host>.md (summary tables).
"""
import argparse
import csv
import glob
import gzip
import os
import platform
import re
import shutil
import socket
import statistics
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# name, kind, encoder command, decoder command, extension, slow?
# {in} / {out} are replaced by file paths. "image" codecs read/write PNM,
# "generic" codecs read the original file and must reproduce it byte by byte.
CODECS = [
    ("jpeg-ls", "image", ["{charls}", "enc", "{in}", "{out}"], ["{charls}", "dec", "{in}", "{out}"], "jls", False),
    ("jpeg-xl-e1", "image", ["cjxl", "{in}", "{out}", "-d", "0", "-e", "1", "--num_threads=1", "--quiet"],
     ["djxl", "{in}", "{out}", "--num_threads=1", "--quiet"], "jxl", False),
    ("jpeg-xl-e3", "image", ["cjxl", "{in}", "{out}", "-d", "0", "-e", "3", "--num_threads=1", "--quiet"],
     ["djxl", "{in}", "{out}", "--num_threads=1", "--quiet"], "jxl", False),
    ("jpeg-xl-e7", "image", ["cjxl", "{in}", "{out}", "-d", "0", "-e", "7", "--num_threads=1", "--quiet"],
     ["djxl", "{in}", "{out}", "--num_threads=1", "--quiet"], "jxl", False),
    ("jpeg-xl-e9", "image", ["cjxl", "{in}", "{out}", "-d", "0", "-e", "9", "--num_threads=1", "--quiet"],
     ["djxl", "{in}", "{out}", "--num_threads=1", "--quiet"], "jxl", True),
    ("png-optipng", "image", ["optipng", "-quiet", "-o2", "-out", "{out}", "{in}"],
     ["sh", "-c", "pngtopnm \"$0\" > \"$1\"", "{in}", "{out}"], "png", False),
    ("zstd-19", "generic", ["zstd", "-q", "-f", "-19", "-T1", "{in}", "-o", "{out}"],
     ["zstd", "-q", "-f", "-d", "{in}", "-o", "{out}"], "zst", False),
    ("xz-9", "generic", ["sh", "-c", "xz -9 -T1 -c \"$0\" > \"$1\"", "{in}", "{out}"],
     ["sh", "-c", "xz -d -c \"$0\" > \"$1\"", "{in}", "{out}"], "xz", False),
    ("bzip2-9", "generic", ["sh", "-c", "bzip2 -9 -c \"$0\" > \"$1\"", "{in}", "{out}"],
     ["sh", "-c", "bzip2 -d -c \"$0\" > \"$1\"", "{in}", "{out}"], "bz2", False),
]

GROUPS = {
    "pgm8": "PGM 8 bpp",
    "ppm": "PPM 24 bpp",
    "kodak": "Kodak (PPM 24 bpp)",
    "raw16": "RAW 16 bpp",
}


# ---------------------------------------------------------------------------
# PNM helpers

def parse_pnm(data):
    """Returns (magic, width, height, maxval, raster bytes) of a binary PNM."""
    if data[:1] != b"P" or data[1:2] not in (b"5", b"6"):
        raise ValueError("not a binary PGM/PPM")
    vals, p = [], 2
    while len(vals) < 3:
        while data[p:p + 1].isspace() or data[p:p + 1] == b"#":
            if data[p:p + 1] == b"#":
                while data[p:p + 1] not in (b"\n", b""):
                    p += 1
            else:
                p += 1
        q = p
        while data[q:q + 1].isdigit():
            q += 1
        vals.append(int(data[p:q]))
        p = q
    p += 1  # single whitespace before the raster
    w, h, maxval = vals
    chans = 3 if data[1:2] == b"6" else 1
    n = w * h * chans * (2 if maxval > 255 else 1)
    return data[:2].decode(), w, h, maxval, data[p:p + n]


def raw_params(path):
    """Dimensions/format of a RAW file from its name: ...-u16be-1xROWSxCOLS.raw"""
    name = os.path.basename(path)
    m = re.search(r"1x(\d+)x(\d+)\.raw$", name)
    if not m:
        raise ValueError(f"cannot get dimensions from the name {name}")
    rows, cols = int(m.group(1)), int(m.group(2))
    bpp = 8 if re.search(r"u8", name) else 16
    big = not re.search(r"le[-_.]", name)
    return rows, cols, bpp, big


def raw_to_pgm(path, dst):
    rows, cols, bpp, big = raw_params(path)
    with open(path, "rb") as f:
        data = f.read()
    n = rows * cols * (bpp // 8)
    if len(data) < n:
        raise ValueError(f"{path}: {len(data)} bytes, expected {n}")
    data = data[:n]
    if bpp == 16 and not big:  # PGM is always big-endian
        b = bytearray(data)
        b[0::2], b[1::2] = data[1::2], data[0::2]
        data = bytes(b)
    with open(dst, "wb") as f:
        f.write(b"P5\n%d %d\n%d\n" % (cols, rows, (1 << bpp) - 1))
        f.write(data)
    return rows, cols


# ---------------------------------------------------------------------------
# Running and measuring

def run_measured(cmd):
    """Runs cmd under /usr/bin/time; returns (wall seconds, max RSS in KB)."""
    with tempfile.NamedTemporaryFile("r", suffix=".time") as tf:
        full = ["/usr/bin/time", "-f", "%M", "-o", tf.name] + cmd
        t0 = time.perf_counter()
        r = subprocess.run(full, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        dt = time.perf_counter() - t0
        if r.returncode != 0:
            raise RuntimeError(f"command failed ({r.returncode}): {' '.join(cmd)}\n{r.stderr.decode(errors='replace')}")
        lines = [l for l in tf.read().splitlines() if l.strip()]
        rss = int(lines[-1]) if lines else 0
    return dt, rss


def fill(template, **kw):
    return [t.format(**kw) for t in template]


def check_tools(codecs, charls):
    needs = {"jpeg-ls": [], "png-optipng": ["optipng", "pngtopnm"], "zstd-19": ["zstd"], "xz-9": ["xz"],
             "bzip2-9": ["bzip2"]}
    ok = []
    for c in codecs:
        name = c[0]
        tools = needs.get(name, ["cjxl", "djxl"])
        missing = [t for t in tools if shutil.which(t) is None]
        if name == "jpeg-ls" and not os.access(charls, os.X_OK):
            missing.append(charls + " (run: make bin/charls_cli)")
        if missing:
            print(f"warning: skipping {name}, missing: {', '.join(missing)}", file=sys.stderr)
        else:
            ok.append(c)
    if not shutil.which("time") and not os.path.exists("/usr/bin/time"):
        sys.exit("error: /usr/bin/time is needed (sudo apt install time)")
    return ok


def collect_images(groups, work):
    """Returns a list of dicts with group, name, original file and PNM file."""
    imgs = []
    os.makedirs(os.path.join(work, "inputs"), exist_ok=True)
    if "pgm8" in groups:
        for f in sorted(glob.glob(os.path.join(ROOT, "imgs-pgm8", "*.pgm"))):
            imgs.append({"group": "pgm8", "name": os.path.basename(f), "orig": f, "pnm": f})
    if "ppm" in groups:
        for f in sorted(glob.glob(os.path.join(ROOT, "imgs-ppm", "*.ppm"))):
            imgs.append({"group": "ppm", "name": os.path.basename(f), "orig": f, "pnm": f})
    if "kodak" in groups:
        for f in sorted(glob.glob(os.path.join(ROOT, "imgs-ppm", "kodak", "*.ppm*"))):
            name = os.path.basename(f)
            if f.endswith(".gz"):
                name = name[:-3]
                dst = os.path.join(work, "inputs", "kodak_" + name)
                with gzip.open(f, "rb") as src, open(dst, "wb") as out:
                    shutil.copyfileobj(src, out)
                f = dst
            imgs.append({"group": "kodak", "name": name, "orig": f, "pnm": f})
    if "raw16" in groups:
        for f in sorted(glob.glob(os.path.join(ROOT, "imgs-raw16", "*.raw"))):
            dst = os.path.join(work, "inputs", os.path.basename(f) + ".pgm")
            raw_to_pgm(f, dst)
            imgs.append({"group": "raw16", "name": os.path.basename(f), "orig": f, "pnm": dst})
    for im in imgs:
        with open(im["pnm"], "rb") as fh:
            _, w, h, _, raster = parse_pnm(fh.read())
        im["pixels"], im["raster"] = w * h, raster
        im["orig_bytes"] = os.path.getsize(im["orig"])
    return imgs


def bench(img, codec, work, repeat, charls):
    name, kind, enc, dec, ext, slow = codec
    src = img["pnm"] if kind == "image" else img["orig"]
    comp = os.path.join(work, f"c.{ext}")
    # djxl picks the output format from the extension (.pgm / .ppm)
    out = os.path.join(work, "d" + os.path.splitext(img["pnm"])[1] if kind == "image" else "d.bin")
    n = 1 if slow else repeat
    te, me, td, md = [], 0, [], 0
    for _ in range(n):
        for f in (comp, out):
            if os.path.exists(f):
                os.remove(f)
        t, m = run_measured(fill(enc, **{"in": src, "out": comp, "charls": charls}))
        te.append(t)
        me = max(me, m)
        t, m = run_measured(fill(dec, **{"in": comp, "out": out, "charls": charls}))
        td.append(t)
        md = max(md, m)
    size = os.path.getsize(comp)
    with open(out, "rb") as fh:
        dec_data = fh.read()
    if kind == "image":
        lossless = parse_pnm(dec_data)[4] == img["raster"]
    else:
        with open(img["orig"], "rb") as fh:
            lossless = fh.read() == dec_data
    return {
        "group": img["group"], "image": img["name"], "codec": name,
        "pixels": img["pixels"], "orig_bytes": img["orig_bytes"], "comp_bytes": size,
        "bpp": 8.0 * size / img["pixels"], "enc_s": min(te), "dec_s": min(td),
        "enc_mem_mb": me / 1024.0, "dec_mem_mb": md / 1024.0, "lossless": lossless,
    }


# ---------------------------------------------------------------------------
# Report

def machine_info():
    cpu = platform.processor() or "?"
    try:
        with open("/proc/cpuinfo") as f:
            for line in f:
                if line.startswith("model name"):
                    cpu = line.split(":", 1)[1].strip()
                    break
    except OSError:
        pass
    return f"{socket.gethostname()}, {cpu}, {os.cpu_count()} CPUs, {platform.system()} {platform.release()}"


def versions():
    cmds = {
        "cjxl": ["cjxl", "--version"], "optipng": ["optipng", "-v"], "zstd": ["zstd", "--version"],
        "xz": ["xz", "--version"], "bzip2": ["bzip2", "--help"],
    }
    out = {}
    for k, c in cmds.items():
        if shutil.which(c[0]):
            r = subprocess.run(c, capture_output=True, text=True)
            txt = (r.stdout + r.stderr).strip().splitlines()
            out[k] = txt[0][:80] if txt else "?"
    return out


def write_markdown(path, rows, codecs, repeat):
    lines = ["# Lossless state-of-the-art benchmark", "",
             f"Machine: {machine_info()}  ", f"Times: minimum of {repeat} run(s) (JPEG-XL -e 9: 1 run), "
             "single thread, wall clock, including file I/O.  ",
             "bpp = compressed bits / number of pixels (a PPM pixel has 3 samples: raw size 24 bpp).", ""]
    lines += ["Versions: " + "; ".join(f"{k}: {v}" for k, v in versions().items()), ""]
    names = [c[0] for c in codecs]
    bad = [r for r in rows if not r["lossless"]]
    if bad:
        lines += ["**WARNING: not lossless:** " + ", ".join(f"{r['codec']}/{r['image']}" for r in bad), ""]
    for g, title in GROUPS.items():
        gr = [r for r in rows if r["group"] == g]
        if not gr:
            continue
        nimg = len({r["image"] for r in gr})
        lines += [f"## {title} ({nimg} images)", "",
                  "| Codec | mean bpp | total bpp | enc time (s) | dec time (s) | enc MB/s | max mem enc / dec (MB) |",
                  "|---|---|---|---|---|---|---|"]
        for n in names:
            cr = [r for r in gr if r["codec"] == n]
            if not cr:
                continue
            mean_bpp = statistics.mean(r["bpp"] for r in cr)
            tot_bpp = 8.0 * sum(r["comp_bytes"] for r in cr) / sum(r["pixels"] for r in cr)
            te, td = sum(r["enc_s"] for r in cr), sum(r["dec_s"] for r in cr)
            mbs = sum(r["orig_bytes"] for r in cr) / 1e6 / te if te else 0
            me, md = max(r["enc_mem_mb"] for r in cr), max(r["dec_mem_mb"] for r in cr)
            lines.append(f"| {n} | {mean_bpp:.3f} | {tot_bpp:.3f} | {te:.2f} | {td:.2f} | {mbs:.1f} | {me:.0f} / {md:.0f} |")
        lines += ["", "bpp per image:", "", "| Image | " + " | ".join(names) + " |",
                  "|---" * (len(names) + 1) + "|"]
        for im in sorted({r["image"] for r in gr}):
            vals = {r["codec"]: r["bpp"] for r in gr if r["image"] == im}
            best = min(vals.values())
            cells = [("**%.3f**" if vals[n] == best else "%.3f") % vals[n] if n in vals else "-" for n in names]
            lines.append(f"| {im} | " + " | ".join(cells) + " |")
        lines.append("")
    with open(path, "w") as f:
        f.write("\n".join(lines))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--groups", nargs="+", default=list(GROUPS), choices=list(GROUPS))
    ap.add_argument("--codecs", nargs="+", default=[c[0] for c in CODECS], choices=[c[0] for c in CODECS])
    ap.add_argument("--repeat", type=int, default=3, help="runs per measurement (minimum time is kept)")
    ap.add_argument("--skip-slow", action="store_true", help="skip JPEG-XL -e 9")
    ap.add_argument("--out", default=os.path.join(ROOT, "results"), help="output directory")
    ap.add_argument("--tag", default=socket.gethostname(), help="suffix of the output files")
    args = ap.parse_args()

    charls = os.path.join(ROOT, "bin", "charls_cli")
    codecs = [c for c in CODECS if c[0] in args.codecs and not (args.skip_slow and c[5])]
    codecs = check_tools(codecs, charls)
    os.makedirs(args.out, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="ic_bench_") as work:
        imgs = collect_images(args.groups, work)
        if not imgs:
            sys.exit("error: no test images found (imgs-pgm8/, imgs-ppm/, imgs-raw16/ in the repository root)")
        rows = []
        total = len(imgs) * len(codecs)
        k = 0
        for img in imgs:
            for c in codecs:
                k += 1
                print(f"[{k}/{total}] {c[0]:12s} {img['group']}/{img['name']}", end="", flush=True)
                r = bench(img, c, work, args.repeat, charls)
                rows.append(r)
                print(f"  {r['bpp']:.3f} bpp  enc {r['enc_s']:.2f}s  dec {r['dec_s']:.2f}s"
                      + ("" if r["lossless"] else "  ** NOT LOSSLESS **"), flush=True)

    base = os.path.join(args.out, f"sota_{args.tag}")
    with open(base + ".csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        for r in rows:
            w.writerow({k: (f"{v:.6f}" if isinstance(v, float) else v) for k, v in r.items()})
    write_markdown(base + ".md", rows, codecs, args.repeat)
    print(f"\nWrote {base}.csv and {base}.md")
    if any(not r["lossless"] for r in rows):
        sys.exit(1)


if __name__ == "__main__":
    main()
