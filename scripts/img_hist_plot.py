#!/usr/bin/env python3
"""Plots the histograms written by img_hist.

Reads <prefix>_hist.csv, <prefix>_diff.csv and <prefix>_stats.csv and writes
<prefix>_hist.png with one row per channel and three columns: sample values,
horizontal differences and vertical differences. Entropies go in the titles.
The difference plots hide the outer 0.1% of the mass (the entropies printed by
img_hist always use the full histograms).
"""
import argparse
import csv
import sys
import warnings

import matplotlib
import matplotlib.ticker

import numpy as np

CHANNEL_COLORS = {"Y": "#4a4a4a", "R": "#c0392b", "G": "#2e8b57", "B": "#2f6db5"}


def load_csv(path):
    with open(path, newline="") as f:
        reader = csv.reader(f)
        header = next(reader)
        rows = [r for r in reader]
    cols = {}
    for i, name in enumerate(header):
        try:
            cols[name] = np.array([float(r[i]) for r in rows])
        except ValueError:
            cols[name] = [r[i] for r in rows]
    return header, cols


def trim(x, y, keep):
    """Restrict the x-range to where the counts are (drops long empty tails)."""
    nz = np.nonzero(y)[0]
    if len(nz) == 0:
        return x, y
    lo, hi = nz[0], nz[-1]
    # Cut the extreme tails holding < (1-keep) of the mass (invisible anyway).
    if keep < 1 and hi - lo > 64:
        c = np.cumsum(y) / y.sum()
        lo = max(lo, int(np.searchsorted(c, (1 - keep) / 2)))
        hi = min(hi, int(np.searchsorted(c, 1 - (1 - keep) / 2)))
    return x[lo:hi + 1], y[lo:hi + 1]


def draw(ax, x, y, color, title, keep=1.0):
    x, y = trim(x, y, keep)
    if len(x) <= 300:
        ax.bar(x, y, width=1.0, color=color, linewidth=0)
    else:
        ax.fill_between(x, y, step="mid", color=color, linewidth=0)
    ax.set_title(title, fontsize=9)
    ax.grid(axis="y", color="#dddddd", linewidth=0.6)
    ax.set_axisbelow(True)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    ax.tick_params(labelsize=7)
    ax.yaxis.set_major_formatter(matplotlib.ticker.FuncFormatter(lambda v, _: f"{v:,.0f}"))
    ax.set_ylim(bottom=0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("prefix")
    ap.add_argument("--title", default="")
    ap.add_argument("--show", action="store_true")
    a = ap.parse_args()

    warnings.filterwarnings("ignore")
    if not a.show:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    _, hist = load_csv(a.prefix + "_hist.csv")
    _, diff = load_csv(a.prefix + "_diff.csv")
    _, stats = load_csv(a.prefix + "_stats.csv")
    channels = stats["channel"]

    fig, axes = plt.subplots(len(channels), 3, figsize=(12, 2.8 * len(channels)), squeeze=False)
    for i, ch in enumerate(channels):
        color = CHANNEL_COLORS.get(ch, "#4a4a4a")
        H, Hh, Hv = stats["H"][i], stats["H_hdiff"][i], stats["H_vdiff"][i]
        draw(axes[i][0], hist["value"], hist[ch], color, f"{ch}: values   H = {H:.3f} bits")
        draw(axes[i][1], diff["diff"], diff[ch + "_h"], color, f"{ch}: horizontal diff   H = {Hh:.3f} bits", 0.999)
        draw(axes[i][2], diff["diff"], diff[ch + "_v"], color, f"{ch}: vertical diff   H = {Hv:.3f} bits", 0.999)
    for i in range(len(channels)):
        axes[i][0].set_ylabel("count", fontsize=8)
    if a.title:
        fig.suptitle(a.title, fontsize=11)
    fig.tight_layout()
    out = a.prefix + "_hist.png"
    fig.savefig(out, dpi=110)
    if a.show:
        plt.show()
    return 0


if __name__ == "__main__":
    sys.exit(main())
