#!/usr/bin/env python3
"""Benchmark of state-of-the-art lossless codecs on the test images (Part II.5).

For every image and codec it measures:
  - the compressed size, as bits per pixel (bpp = 8 * bytes / (rows * cols));
  - encoding and decoding wall-clock time (minimum over --repeat runs);
  - peak memory (max RSS, from /usr/bin/time);
and checks that decoding gives back exactly the original samples.

Codecs:
  ours           : bin/img_lossless_enc / img_lossless_dec (gets the original file,
                   with -rows/-cols/-bpp/-be|-le for RAW images); ours-fast and
                   ours-fastest use the speed profiles; ours-mt codes one stripe per
                   CPU in parallel (the only multi-threaded entry)
  image specific : JPEG-LS (CharLS, via bin/charls_cli), JPEG-XL lossless
                   (cjxl -d 0, several efforts), PNG (optipng -o2)
  general purpose: zstd -19, xz -9, bzip2 -9 (applied to the original file)

All codecs run single-threaded (except ours-mt), so they are comparable.

Image codecs need a PNM file: 16-bit RAW images are converted to a 16-bit
PGM first (same samples, conversion not timed); gzipped PPMs (kodak/) are
decompressed first. General-purpose compressors get the original file.

Usage (from the repository root, after `make bin/charls_cli`):
  python3 scripts/benchmark_sota.py                 # everything
  python3 scripts/benchmark_sota.py --skip-slow     # without JPEG-XL -e 9
  python3 scripts/benchmark_sota.py --repeat 3 --groups pgm8 ppm
  python3 scripts/benchmark_sota.py --codecs ours --merge results/sota_<host>.csv --tag ours
                                                    # only our codec, compared with an earlier run
Results go to results/sota_<host>.csv (one row per image x codec),
results/sota_<host>_summary.csv (averages per group x codec) and
results/sota_<host>.md (summary tables).
  python3 scripts/benchmark_sota.py --from-csv results/sota_<host>.csv
rebuilds the summaries from an existing per-image CSV without running anything.
"""
import argparse
import csv
import glob
import gzip
import math
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
    # our codec: reads the original file (RAW parameters given as options)
    ("ours", "ours", ["{bin}/img_lossless_enc", "{raw}", "{in}", "{out}"], ["{bin}/img_lossless_dec", "{in}", "{out}"],
     "icl", False),
    ("ours-fast", "ours", ["{bin}/img_lossless_enc", "-fast", "{raw}", "{in}", "{out}"],
     ["{bin}/img_lossless_dec", "{in}", "{out}"], "icl", False),
    ("ours-fastest", "ours", ["{bin}/img_lossless_enc", "-fastest", "{raw}", "{in}", "{out}"],
     ["{bin}/img_lossless_dec", "{in}", "{out}"], "icl", False),
    # stripes coded in parallel, one per CPU of the machine
    ("ours-mt", "ours", ["{bin}/img_lossless_enc", "-t", str(os.cpu_count() or 1), "{raw}", "{in}", "{out}"],
     ["{bin}/img_lossless_dec", "{in}", "{out}"], "icl", False),
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


def fill(template, raw=(), **kw):
    out = []
    for t in template:
        if t == "{raw}":
            out.extend(raw)
        else:
            out.append(t.format(bin=os.path.join(ROOT, "bin"), **kw))
    return out


def check_tools(codecs, charls):
    needs = {"ours": [], "ours-fast": [], "ours-fastest": [], "ours-mt": [], "jpeg-ls": [], "png-optipng": ["optipng", "pngtopnm"], "zstd-19": ["zstd"], "xz-9": ["xz"],
             "bzip2-9": ["bzip2"]}
    ok = []
    for c in codecs:
        name = c[0]
        tools = needs.get(name, ["cjxl", "djxl"])
        missing = [t for t in tools if shutil.which(t) is None]
        if name == "jpeg-ls" and not os.access(charls, os.X_OK):
            missing.append(charls + " (run: make bin/charls_cli)")
        if name.startswith("ours") and not os.access(os.path.join(ROOT, "bin", "img_lossless_enc"), os.X_OK):
            missing.append("bin/img_lossless_enc (run: make)")
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
            rows, cols, bpp, big = raw_params(f)
            raw_to_pgm(f, dst)
            imgs.append({"group": "raw16", "name": os.path.basename(f), "orig": f, "pnm": dst,
                         "rawopts": ["-rows", str(rows), "-cols", str(cols), "-bpp", str(bpp),
                                     "-be" if big else "-le"]})
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
        t, m = run_measured(fill(enc, img.get("rawopts", []), **{"in": src, "out": comp, "charls": charls}))
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


REFERENCE = "jpeg-ls"  # codec used as the baseline of the relative metric


def summarize(rows, names):
    """Averages per (group, codec), plus an "all" group across every image.

    mean_bpp       arithmetic mean of the per-image bpp (every image weighs the same)
    total_bpp      all compressed bits / all pixels (large images weigh more)
    vs_jpegls_pct  geometric mean over the images of size(codec) / size(JPEG-LS),
                   as a percentage change: -10 means 10% smaller than JPEG-LS.
                   It does not depend on the image size or bit depth, so it can
                   also be averaged across groups ("all").
    wins           images where the codec has the smallest file
    enc/dec_mpix_s throughput: all pixels / total time (megapixels per second)
    """
    best = {}
    for r in rows:
        k = (r["group"], r["image"])
        best[k] = min(best.get(k, float("inf")), r["comp_bytes"])
    ref = {(r["group"], r["image"]): r["comp_bytes"] for r in rows if r["codec"] == REFERENCE}
    out = []
    for g in list(GROUPS) + ["all"]:
        gr = [r for r in rows if g == "all" or r["group"] == g]
        if not gr or (g == "all" and len({r["group"] for r in rows}) < 2):
            continue
        for n in names:
            cr = [r for r in gr if r["codec"] == n]
            if not cr:
                continue
            pix = sum(r["pixels"] for r in cr)
            te, td = sum(r["enc_s"] for r in cr), sum(r["dec_s"] for r in cr)
            logs = [math.log(r["comp_bytes"] / ref[(r["group"], r["image"])])
                    for r in cr if (r["group"], r["image"]) in ref]
            out.append({
                "group": g, "codec": n, "images": len(cr),
                "mean_bpp": statistics.mean(r["bpp"] for r in cr) if g != "all" else None,
                "total_bpp": 8.0 * sum(r["comp_bytes"] for r in cr) / pix if g != "all" else None,
                "vs_jpegls_pct": 100.0 * (math.exp(statistics.mean(logs)) - 1) if logs else None,
                "wins": sum(1 for r in cr if r["comp_bytes"] == best[(r["group"], r["image"])]),
                "enc_mpix_s": pix / 1e6 / te if te else None,
                "dec_mpix_s": pix / 1e6 / td if td else None,
                "enc_s_total": te, "dec_s_total": td,
                "enc_mem_mb_max": max(r["enc_mem_mb"] for r in cr),
                "dec_mem_mb_max": max(r["dec_mem_mb"] for r in cr),
            })
    return out


def write_summary_csv(path, summary):
    with open(path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(summary[0].keys()))
        w.writeheader()
        for s in summary:
            w.writerow({k: ("" if v is None else f"{v:.4f}" if isinstance(v, float) else v) for k, v in s.items()})


def fmt(v, spec):
    return "-" if v is None else format(v, spec)


def write_markdown(path, rows, names, header):
    lines = ["# Lossless state-of-the-art benchmark", ""] + header + [
        "bpp = compressed bits / number of pixels (a PPM pixel has 3 samples: raw size 24 bpp).  ",
        "mean bpp: every image weighs the same; total bpp: all bits / all pixels.  ",
        f"vs JPEG-LS: geometric mean of the per-image size ratio to JPEG-LS (negative = smaller).  ",
        "Mpix/s: megapixels per second (all pixels / total time).", ""]
    summary = summarize(rows, names)
    bad = [r for r in rows if not r["lossless"]]
    if bad:
        lines += ["**WARNING: not lossless:** " + ", ".join(f"{r['codec']}/{r['image']}" for r in bad), ""]
    for g, title in GROUPS.items():
        gr = [r for r in rows if r["group"] == g]
        if not gr:
            continue
        nimg = len({r["image"] for r in gr})
        lines += [f"## {title} ({nimg} images)", ""] + summary_table([s for s in summary if s["group"] == g])
        lines += ["", "bpp per image:", "", "| Image | " + " | ".join(names) + " |",
                  "|---" * (len(names) + 1) + "|"]
        for im in sorted({r["image"] for r in gr}):
            vals = {r["codec"]: r["bpp"] for r in gr if r["image"] == im}
            best = min(vals.values())
            cells = [("**%.3f**" if vals[n] == best else "%.3f") % vals[n] if n in vals else "-" for n in names]
            lines.append(f"| {im} | " + " | ".join(cells) + " |")
        lines.append("")
    allg = [s for s in summary if s["group"] == "all"]
    if allg:
        lines += [f"## All images ({allg[0]['images']})", "",
                  "bpp is not averaged across groups (8, 16 and 24 bpp images); use the relative column.", ""]
        lines += summary_table(allg) + [""]
    with open(path, "w") as f:
        f.write("\n".join(lines))
    return summary


def summary_table(summary):
    t = ["| Codec | mean bpp | total bpp | vs JPEG-LS | wins | enc Mpix/s | dec Mpix/s | max mem enc / dec (MB) |",
         "|---|---|---|---|---|---|---|---|"]
    for s in summary:
        t.append(f"| {s['codec']} | {fmt(s['mean_bpp'], '.3f')} | {fmt(s['total_bpp'], '.3f')} | "
                 f"{fmt(s['vs_jpegls_pct'], '+.1f')}% | {s['wins']}/{s['images']} | {fmt(s['enc_mpix_s'], '.2f')} | "
                 f"{fmt(s['dec_mpix_s'], '.2f')} | {s['enc_mem_mb_max']:.0f} / {s['dec_mem_mb_max']:.0f} |")
    return t


def read_rows(path):
    rows = []
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            for k in ("pixels", "orig_bytes", "comp_bytes"):
                r[k] = int(r[k])
            for k in ("bpp", "enc_s", "dec_s", "enc_mem_mb", "dec_mem_mb"):
                r[k] = float(r[k])
            r["lossless"] = r["lossless"] == "True"
            rows.append(r)
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--groups", nargs="+", default=list(GROUPS), choices=list(GROUPS))
    ap.add_argument("--codecs", nargs="+", default=[c[0] for c in CODECS], choices=[c[0] for c in CODECS])
    ap.add_argument("--repeat", type=int, default=3, help="runs per measurement (minimum time is kept)")
    ap.add_argument("--skip-slow", action="store_true", help="skip JPEG-XL -e 9")
    ap.add_argument("--out", default=os.path.join(ROOT, "results"), help="output directory")
    ap.add_argument("--tag", default=socket.gethostname(), help="suffix of the output files")
    ap.add_argument("--merge", metavar="CSV",
                    help="add the rows of an earlier run (e.g. the reference codecs) for codecs not run now")
    ap.add_argument("--from-csv", metavar="CSV",
                    help="do not run anything: rebuild the .md and _summary.csv from a per-image CSV")
    args = ap.parse_args()

    if args.from_csv:
        rows = read_rows(args.from_csv)
        present = {r["codec"] for r in rows}
        names = [c[0] for c in CODECS if c[0] in present] + sorted(present - {c[0] for c in CODECS})
        base = os.path.splitext(args.from_csv)[0]
        write_markdown(base + ".md", rows, names,
                       [f"Rebuilt from {os.path.basename(args.from_csv)} (machine and versions: see the original run).", ""])
        write_summary_csv(base + "_summary.csv", summarize(rows, names))
        print(f"Wrote {base}.md and {base}_summary.csv")
        return

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

    names = [c[0] for c in codecs]
    if args.merge:
        old_rows = [r for r in read_rows(args.merge) if r["codec"] not in names]
        imgs_now = {(r["group"], r["image"]) for r in rows}
        rows += [r for r in old_rows if (r["group"], r["image"]) in imgs_now]
        names += [c[0] for c in CODECS if c[0] in {r["codec"] for r in old_rows} and c[0] not in names]
        rows.sort(key=lambda r: (list(GROUPS).index(r["group"]), r["image"], names.index(r["codec"])))
    base = os.path.join(args.out, f"sota_{args.tag}")
    with open(base + ".csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        for r in rows:
            w.writerow({k: (f"{v:.6f}" if isinstance(v, float) else v) for k, v in r.items()})
    header = [f"Machine: {machine_info()}  ",
              f"Times: minimum of {args.repeat} run(s) (JPEG-XL -e 9: 1 run), single thread, wall clock, "
              "including file I/O.  ",
              "Versions: " + "; ".join(f"{k}: {v}" for k, v in versions().items()), ""]
    if args.merge:
        header.insert(-1, f"Rows of codecs not run now taken from {os.path.basename(args.merge)}.  ")
    summary = write_markdown(base + ".md", rows, names, header)
    write_summary_csv(base + "_summary.csv", summary)
    print(f"\nWrote {base}.csv, {base}_summary.csv and {base}.md")
    if any(not r["lossless"] for r in rows):
        sys.exit(1)


if __name__ == "__main__":
    main()
