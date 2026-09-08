#!/usr/bin/env python3
"""Render a canonical benchmark summary.csv as readable Markdown tables."""

import argparse
import csv
from pathlib import Path


def markdown_text(value: str) -> str:
    return value.replace("|", "\\|").replace("\n", " ")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("summary_csv", type=Path)
    parser.add_argument("summary_md", type=Path)
    args = parser.parse_args()

    with args.summary_csv.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))

    required = {"input", "n", "algorithm", "median_us"}
    fields = set(rows[0]) if rows else set()
    missing = required - fields
    if missing:
        names = ", ".join(sorted(missing))
        raise SystemExit(f"{args.summary_csv}: missing required columns: {names}")

    preferred_order = [
        "simulated-direct_live-phase",
        "noalloc",
        "strict-noalloc",
    ]
    present = {row["algorithm"] for row in rows}
    algorithms = [name for name in preferred_order if name in present]
    algorithms += sorted(present - set(algorithms) - {"std::sort"})
    sizes = sorted({int(row["n"]) for row in rows})
    preferred_input_order = [
        "Random",
        "Sorted",
        "Reverse",
        "Sorted+Noise(5%)",
        "Sorted+Noise(10%)",
        "Random%25",
        "Alternating",
        "Sawtooth",
        "MixedDirectionRuns",
        "BlockSorted",
        "OrganPipe",
        "Rotated",
        "MixedPhase3",
        "MixedPhase12",
    ]
    present_inputs = {row["input"] for row in rows}
    inputs = [name for name in preferred_input_order if name in present_inputs]
    inputs += sorted(present_inputs - set(inputs))
    indexed = {(row["input"], int(row["n"]), row["algorithm"]): row for row in rows}

    lines = [
        "# Canonical benchmark summary",
        "",
        "Cells show `std::sort median / JesseSort median`; higher is better. Ratios use independent medians.",
        "",
    ]
    for size in sizes:
        lines.extend(
            [
                f"## n={size}",
                "",
                "| Input | " + " | ".join(map(markdown_text, algorithms)) + " |",
                "|---|" + "|".join("---:" for _ in algorithms) + "|",
            ]
        )
        for input_name in inputs:
            std_row = indexed.get((input_name, size, "std::sort"))
            std_median = float(std_row["median_us"]) if std_row else None
            cells = []
            for algorithm in algorithms:
                row = indexed.get((input_name, size, algorithm))
                if row is None:
                    cells.append("—")
                    continue
                if std_median is None or std_median == 0:
                    cells.append("—")
                else:
                    median = float(row["median_us"])
                    cells.append(f"{std_median / median:.4f}")
            lines.append(f"| {markdown_text(input_name)} | " + " | ".join(cells) + " |")
        lines.append("")

    args.summary_md.write_text("\n".join(lines), encoding="utf-8")


if __name__ == "__main__":
    main()
