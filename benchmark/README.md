This folder provides benchmarking utilities, currently mostly covering the backend model.
The aim is to quantify performance improvements on a wide range of synthetic but hopefully somewhat realistic scenarios.


The benchmark is run manually and is not registered with CTest. With `BUILD_TESTING=ON`, build and run it from the repository root, redirecting stdout to a file:
```
cmake --build build --target timelinemodelbenchmark -j4
QT_QPA_PLATFORM=offscreen build/bin/timelinemodelbenchmark > benchmark/baseline.csv
```

Then, from the repository root, compare two measurements using Python 3 (no additional packages required):
```
python3 benchmark/compare.py benchmark/base.csv benchmark/improved.csv
```
This will print the speedup for each operation, in decreasing order. Higher is better. Ideally we'd want to see everything > 1.0, but in practice that's rarely the case. We can make tradeoff where we drastically improve the perf of a function that is called very often, while losing out a little on one that is more rarely called.
