#!/usr/bin/env python3
"""Tabulates scripts/feature-costs.sh runs: per variant, the mean over its
passes of the frame rate, the 1% low, the CPU render time per frame, and
the average and p99 GPU time of each stage; with --base, each variant's
difference from that variant (what switching the feature off saves).

    python3 scripts/feature-costs.py <out-dir> [--base <label>] [--md]
"""
import glob
import os
import re
import statistics
import sys

STAGES = ["whole frame", "fog volume", "light bake", "light averages", "world pass",
          "ambient shadows", "bloom", "blit and 2D", "output"]
SHORT = {"whole frame": "frame", "fog volume": "fog", "light bake": "bake", "light averages": "avgs",
         "world pass": "world", "ambient shadows": "ao", "bloom": "bloom", "blit and 2D": "blit",
         "output": "out"}


def read_run(prefix):
    run = {}
    try:
        s = open(prefix + ".csv.summary.txt").read()
    except OSError:
        return None
    m = re.search(r"average fps: ([\d.]+)", s)
    run["fps"] = float(m.group(1)) if m else float("nan")
    m = re.search(r"1% low \(p99 frame time\): ([\d.]+) fps", s)
    run["low"] = float(m.group(1)) if m else float("nan")
    try:
        g = open(prefix + ".csv.gpu.txt").read()
        for st in STAGES:
            m = re.search(re.escape(st) + r"\s+average\s+([\d.]+) ms\s+p99\s+([\d.]+) ms", g)
            if m:
                run[st] = float(m.group(1))
                run[st + " p99"] = float(m.group(2))
    except OSError:
        pass
    # CPU render time per frame, from the per-frame CSV (after the first 2 s)
    try:
        rows = open(prefix + ".csv").read().splitlines()
        head = rows[0].split(",")
        ti, ri = head.index("time_s"), head.index("render_ms")
        vals = [float(r.split(",")[ri]) for r in rows[1:] if r and float(r.split(",")[ti]) >= 2.0]
        run["cpu render"] = statistics.mean(vals) if vals else float("nan")
    except (OSError, ValueError, IndexError):
        pass
    return run


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return
    out = args[0]
    base = args[args.index("--base") + 1] if "--base" in args else None
    md = "--md" in args
    runs = {}
    order = []
    for path in sorted(glob.glob(os.path.join(out, "*.*.csv.summary.txt")), key=os.path.getmtime):
        prefix = path[: -len(".csv.summary.txt")]
        label = os.path.basename(prefix).rsplit(".", 1)[0]
        r = read_run(prefix)
        if r is None:
            continue
        if label not in runs:
            runs[label] = []
            order.append(label)
        runs[label].append(r)

    def mean(label, key):
        vals = [r[key] for r in runs[label] if key in r]
        return statistics.mean(vals) if vals else float("nan")

    keys = ["fps", "low", "cpu render"] + STAGES + ["world pass p99", "ambient shadows p99", "whole frame p99"]
    names = ["fps", "1%low", "cpu"] + [SHORT[s] for s in STAGES] + ["world99", "ao99", "frame99"]
    if md:
        print("| variant | n | " + " | ".join(names) + " |")
        print("|---|---:|" + "---:|" * len(names))
    else:
        print("%-18s %2s " % ("variant", "n") + " ".join("%7s" % n for n in names))
    for label in order:
        vals = [mean(label, k) for k in keys]
        cells = ["%.1f" % v if k in ("fps", "low") else "%.2f" % v for k, v in zip(keys, vals)]
        if md:
            print("| %s | %d | " % (label, len(runs[label])) + " | ".join(cells) + " |")
        else:
            print("%-18s %2d " % (label, len(runs[label])) + " ".join("%7s" % c for c in cells))
    if base and base in runs:
        print()
        print("Saved by switching off, against %s (GPU ms per frame; positive = cheaper):" % base)
        bk = ["whole frame", "world pass", "ambient shadows", "fog volume", "light bake", "light averages",
              "bloom", "blit and 2D", "output", "cpu render"]
        if md:
            print("| variant | fps | frame | world | ao | fog | bake | avgs | bloom | blit | out | cpu |")
            print("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
        for label in order:
            if label == base:
                continue
            d = [mean(base, k) - mean(label, k) for k in bk]
            fps = mean(label, "fps") - mean(base, "fps")
            if md:
                print("| %s | %+.1f | " % (label, fps) + " | ".join("%+.2f" % x for x in d) + " |")
            else:
                print("%-18s fps %+6.1f  " % (label, fps) + " ".join("%7s" % ("%+.2f" % x) for x in d))


main()
