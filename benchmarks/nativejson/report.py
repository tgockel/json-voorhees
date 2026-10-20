#!/usr/bin/env python3
"""Print a nativejson-benchmark performance CSV as one table per operation.

    benchmarks/nativejson/report.py [--memory] [CSV or directory ...]

With no arguments, it reads the newest performance_*.csv in build/nativejson/run. Each table has a
row per library, fastest first, and a column per data file plus their total. A library that does not
implement an operation is left out of that table. --memory adds a table of peak memory for Parse.
The output is Markdown, so it pastes into an issue or pull request as it is.
"""

import argparse
import csv
import pathlib
import sys

DEFAULT_DIR = pathlib.Path(__file__).resolve().parent.parent.parent / "build" / "nativejson" / "run"


def find_csv(paths):
    if not paths:
        paths = [DEFAULT_DIR]
    found = []
    for path in map(pathlib.Path, paths):
        if path.is_dir():
            candidates = sorted(path.glob("performance_*.csv"), key=lambda p: p.stat().st_mtime)
            if not candidates:
                sys.exit(f"no performance_*.csv in {path}; run nativejson-benchmark --performance-only first")
            found.append(candidates[-1])
        elif path.is_file():
            found.append(path)
        else:
            sys.exit(f"{path} does not exist")
    return found


def read_rows(path):
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            # "1. Parse" -> 1, "Parse"
            number, row["Type"] = row["Type"].split(". ", 1)
            row["Number"] = int(number)
            yield row


def table(title, unit, headers, rows):
    """rows is [(library, [value or None, ...])]; prints a Markdown table with right-aligned numbers."""
    cells = [[lib] + ["—" if v is None else unit(v) for v in values] for lib, values in rows]
    header = ["Library"] + headers
    widths = [max(len(r[i]) for r in [header] + cells) for i in range(len(header))]

    def line(r):
        return "| " + " | ".join(c.ljust(w) if i == 0 else c.rjust(w)
                                 for i, (c, w) in enumerate(zip(r, widths))) + " |"

    print(f"### {title}\n")
    print(line(header))
    print("|" + "|".join(("-" * (w + 1)) + (":" if i else "-") for i, w in enumerate(widths)) + "|")
    for r in cells:
        print(line(r))
    print()


def report(path, show_memory):
    rows = list(read_rows(path))
    print(f"## {path.name}\n")

    # In upstream's order, which the first library listed need not have all of.
    operations = sorted({(row["Number"], row["Type"]) for row in rows if row["Type"] != "Code size"})
    files = sorted({row["Filename"] for row in rows if row["Type"] != "Code size"})

    def per_file(operation, column, convert):
        by_library = {}
        for row in rows:
            if row["Type"] == operation:
                by_library.setdefault(row["Library"], {})[row["Filename"]] = convert(row[column])
        result = []
        for library, values in by_library.items():
            cols = [values.get(f) for f in files]
            total = sum(v for v in cols if v is not None)
            result.append((library, cols + [total]))
        return sorted(result, key=lambda r: r[1][-1])

    for _, operation in operations:
        table(f"{operation} (ms)", lambda v: f"{v:.2f}", files + ["total"],
              per_file(operation, "Time (ms)", float))

    if show_memory:
        table("Parse peak memory (MiB)", lambda v: f"{v / (1 << 20):.1f}", files + ["total"],
              per_file("Parse", "MemoryPeak (byte)", int))

    sizes = sorted(((row["Library"], [int(row["FileSize (byte)"])]) for row in rows if row["Type"] == "Code size"),
                   key=lambda r: r[1][0])
    if sizes:
        table("Code size (KiB)", lambda v: f"{v / 1024:.0f}", ["jsonstat"], sizes)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--memory", action="store_true", help="also show peak memory while parsing")
    parser.add_argument("paths", nargs="*", help="performance CSV files, or directories to take the newest from")
    args = parser.parse_args()
    for path in find_csv(args.paths):
        report(path, args.memory)


if __name__ == "__main__":
    main()
