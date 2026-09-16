#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
import csv
import math
import sys
from collections import defaultdict
from statistics import median


def medians(path):
    samples = defaultdict(list)
    with open(path, newline="") as source:
        for row in csv.DictReader(line for line in source if not line.startswith("#")):
            samples[row["scenario"], row["size"]].append(float(row["per_operation_ns"]))
    return {key: median(values) for key, values in samples.items()}


if len(sys.argv) != 3:
    sys.exit(f"Usage: {sys.argv[0]} OLD.csv NEW.csv")

old, new = medians(sys.argv[1]), medians(sys.argv[2])
results = []
for key in sorted(old.keys() | new.keys()):
    operation = f"{key[0]} (size={key[1]})"
    if key not in old or key not in new:
        print(f"Skipping unmatched operation: {operation}", file=sys.stderr)
        continue
    if not all(math.isfinite(value) and value > 0 for value in (old[key], new[key])):
        print(f"Skipping invalid timing: {operation}", file=sys.stderr)
        continue
    results.append((old[key] / new[key], operation))

if not results:
    sys.exit("No comparable operations found")

print("SPEEDUP\tOPERATION")
for speedup, operation in sorted(results, key=lambda result: (-result[0], result[1])):
    print(f"{speedup:.3f}x\t{operation}")
