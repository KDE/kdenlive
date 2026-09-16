This folder provides benchmarking utilities, currently mostly covering the backend model.
The aim is to quantify performance improvements on a wide range of synthetic but hopefully somewhat realistic scenarios.


To run it, simply build then call the benchmark executable while redirecting the stdout to a file:
```
bin/timelinemodelbenchmark > ../benchmark/baseline.csv
```

Then, from the repository root, compare two measurements using Python 3 (no additional packages required):
```
python3 benchmark/compare.py benchmark/base.csv benchmark/improved.csv
```
This will print the speedup for each operation, in decreasing order. Higher is better. Ideally we'd want to see everything > 1.0, but in practice that's rarely the case. We can make tradeoff where we drastically improve the perf of a function that is called very often, while loosing out a little on one that is more rarely called.
