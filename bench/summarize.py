#!/usr/bin/env python3
"""Summarize the output of bench/bench.cc, and of bench/ab.sh.

    summarize.py PATH...             the best time per row over the runs
    summarize.py --ab DIR            A against B (DIR holds a_N.txt and b_N.txt, as ab.sh writes them)
    summarize.py --ab DIR_A DIR_B    A against B from two directories of run_N.txt
    summarize.py --readme PATH...    the README's base58 table (from the "compare" tables)

A PATH is a directory (its run_N.txt files), a file of bench.cc output, or a glob pattern.
Times are nanoseconds per call. The best time of a row is its minimum over the runs (or the
median with --stat median); the spread of a row is (max - min) / min over the runs.
"""

from __future__ import annotations

import argparse
import glob
import statistics
import sys
from pathlib import Path

# (codec, bytes, metric) -> the value of each run. Codec rows (the first table) give the metrics
# enc, dec, enc_reuse and dec_reuse; the "compare" table gives the codec "compare" and the
# metrics below, in the order bench.cc prints them.
Key = tuple[str, int, str]
Samples = dict[Key, list[float]]

CODEC_METRICS = ((2, "enc"), (4, "dec"), (6, "enc_reuse"), (7, "dec_reuse"))
COMPARE_METRICS = (
    "enc_bx",
    "enc_reuse",
    "enc_classic",
    "enc_libbase58",
    "enc_firedancer",
    "dec_bx",
    "dec_classic",
    "dec_libbase58",
    "dec_firedancer",
    "dec_reuse",
)
# bench.cc prints times with one decimal, so one unit in the last place is its resolution.
RESOLUTION = 0.1

README_SIZES = (32, 64, 256)
README_COLUMNS = ("bx", "reuse", "firedancer", "libbase58")


def parse_file(path: Path, samples: Samples) -> None:
    """Add the rows of one bench.cc output to samples."""
    with path.open(encoding="utf-8", errors="replace") as f:
        for line in f:
            if "|" in line:
                cells = [c for part in line.split("|") for c in part.split()]
                if len(cells) != 11 or not cells[0].isdigit():
                    continue
                keys = [("compare", int(cells[0]), m) for m in COMPARE_METRICS]
                values = cells[1:]
            else:
                parts = line.split()
                if len(parts) != 8 or "::" not in parts[0] or not parts[1].isdigit():
                    continue
                keys = [(parts[0], int(parts[1]), m) for _, m in CODEC_METRICS]
                values = [parts[i] for i, _ in CODEC_METRICS]
            for key, value in zip(keys, values):
                if value == "-":
                    continue
                try:
                    samples.setdefault(key, []).append(float(value))
                except ValueError:
                    break


def expand(paths: list[str], pattern: str = "run_*.txt") -> list[Path]:
    """The files named by paths: a directory gives its files matching pattern, a glob its matches."""
    files: list[Path] = []
    for p in paths:
        path = Path(p)
        if path.is_dir():
            files += sorted(path.glob(pattern))
        elif any(c in p for c in "*?["):
            files += sorted(Path(m) for m in glob.glob(p))
        elif path.is_file():
            files.append(path)
        else:
            sys.exit(f"summarize.py: {p}: no such file or directory")
    if not files:
        sys.exit(f"summarize.py: no {pattern} files in {' '.join(paths)}")
    return files


def load(paths: list[str], pattern: str = "run_*.txt") -> Samples:
    samples: Samples = {}
    for f in expand(paths, pattern):
        parse_file(f, samples)
    return samples


def best(values: list[float], stat: str) -> float:
    return min(values) if stat == "min" else statistics.median(values)


def spread(values: list[float]) -> float | None:
    """(max - min) / min of the runs, or None for a single run."""
    if len(values) < 2 or min(values) <= 0:
        return None
    return (max(values) - min(values)) / min(values)


def percent(x: float | None) -> str:
    return f"{'-':>7}" if x is None else f"{x * 100:6.1f}%"


def selected(samples: Samples, sizes: list[int], filters: list[str]) -> list[Key]:
    """The keys with a wanted size whose codec name contains one of the filters."""
    keys = [
        k
        for k in samples
        if (not sizes or k[1] in sizes)
        and (not filters or any(f in k[0] for f in filters))
    ]
    return sorted(keys, key=lambda k: (k[0] == "compare", k[0], k[1], k[2]))


def print_runs(samples: Samples, keys: list[Key], stat: str) -> None:
    print(f"{'codec':<17}{'bytes':>6} {'metric':<15}{'ns':>12} {'spread':>7}")
    for key in keys:
        print(
            f"{key[0]:<17}{key[1]:>6} {key[2]:<15}{best(samples[key], stat):12.1f} "
            f"{percent(spread(samples[key]))}"
        )


def print_ab(
    a: Samples, b: Samples, keys: list[Key], stat: str, threshold: float
) -> None:
    print(
        f"{'codec':<17}{'bytes':>6} {'metric':<15}{'A':>10} {'spread':>7} "
        f"{'B':>10} {'spread':>7} {'B/A':>7}  verdict"
    )
    counts = {"faster": 0, "slower": 0, "": 0}
    worst = 0.0
    for key in keys:
        x, y = best(a[key], stat), best(b[key], stat)
        sa, sb = spread(a[key]), spread(b[key])
        ratio = y / x
        # A row counts when B/A is off 1 by more than the spreads of both sides, the threshold, and
        # the resolution of the printed times.
        noise = max(threshold, sa or 0.0, sb or 0.0, RESOLUTION / min(x, y))
        verdict = (
            "faster" if ratio < 1 - noise else "slower" if ratio > 1 + noise else ""
        )
        counts[verdict] += 1
        worst = max(worst, abs(ratio - 1))
        print(
            f"{key[0]:<17}{key[1]:>6} {key[2]:<15}{x:10.1f} {percent(sa)} "
            f"{y:10.1f} {percent(sb)} {ratio:7.3f}  {verdict}".rstrip()
        )
    same = counts[""]
    print(
        f"\n{counts['faster']} faster, {counts['slower']} slower, {same} within the noise "
        f"(more than {threshold * 100:.1f}% and the spreads); "
        f"largest |B/A - 1| is {worst * 100:.1f}%. B/A below 1 means B is faster. "
        "On an A/A run every row should be within the noise."
    )


def fmt_readme(x: float | None) -> str:
    if x is None:
        return "-"
    return f"{x:,.0f}" if x >= 100 else f"{x:.1f}"


def print_readme(samples: Samples, sizes: list[int], stat: str) -> None:
    print("| bytes | operation | base-x | into a string | Firedancer | libbase58 |")
    print("|---:|---|---:|---:|---:|---:|")
    for size in sizes:
        for op, name in (("enc", "encode"), ("dec", "decode")):
            cells = []
            for column in README_COLUMNS:
                values = samples.get(("compare", size, f"{op}_{column}"))
                cells.append(fmt_readme(best(values, stat) if values else None))
            print(f"| {size} | {name} | " + " | ".join(cells) + " |")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Summarize the output of bench/bench.cc and bench/ab.sh.",
        epilog="Times are nanoseconds per call. See docs/BENCHMARKING.md.",
    )
    parser.add_argument(
        "paths",
        nargs="+",
        metavar="PATH",
        help="directories of run_N.txt, bench.cc output files or globs",
    )
    parser.add_argument(
        "--ab",
        action="store_true",
        help="compare A with B: one ab.sh directory (a_N.txt, b_N.txt) or two directories",
    )
    parser.add_argument(
        "--readme", action="store_true", help="print the README's base58 table"
    )
    parser.add_argument(
        "--stat",
        choices=("min", "median"),
        default="min",
        help="the statistic over runs (default: min)",
    )
    parser.add_argument(
        "--sizes",
        help="byte sizes to show, comma separated (--readme default: 32,64,256)",
    )
    parser.add_argument(
        "--filter",
        help="codec names to show, comma separated, by substring (compare: the comparison table)",
    )
    parser.add_argument(
        "--threshold",
        type=float,
        default=3.0,
        help="with --ab, the percent a row must differ by to count (default: 3)",
    )
    args = parser.parse_args()

    sizes = [int(s) for s in (args.sizes or "").replace(",", " ").split()]
    filters = (args.filter or "").replace(",", " ").split()
    if args.ab and args.readme:
        parser.error("--ab and --readme do not combine")
    if args.ab:
        if len(args.paths) == 1:
            a, b = load(args.paths, "a_*.txt"), load(args.paths, "b_*.txt")
        elif len(args.paths) == 2:
            a, b = load([args.paths[0]]), load([args.paths[1]])
        else:
            parser.error("--ab takes one ab.sh directory or two directories")
        keys = [k for k in selected(a, sizes, filters) if k in b]
        print_ab(a, b, keys, args.stat, args.threshold / 100)
        return
    samples = load(args.paths)
    if args.readme:
        print_readme(samples, sizes or list(README_SIZES), args.stat)
        return
    print_runs(samples, selected(samples, sizes, filters), args.stat)


if __name__ == "__main__":
    main()
