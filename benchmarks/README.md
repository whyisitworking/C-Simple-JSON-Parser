# Benchmark Notes

The benchmark harness is intentionally small and portable. It parses the bundled
sample corpus repeatedly and reports throughput in MB/s:

```sh
cmake -S . -B build
cmake --build build
./build/csjp_bench 1000
```

Smoke run on this workspace, using AppleClang via CMake and 10 iterations:

```text
sample/simple.json: 0.00 MB in 0.0000s = 10.90 MB/s
sample/multidim_arr.json: 0.00 MB in 0.0000s = 27.08 MB/s
sample/random.json: 0.00 MB in 0.0000s = 30.95 MB/s
sample/food.json: 0.32 MB in 0.0077s = 41.55 MB/s
sample/rickandmorty.json: 0.19 MB in 0.0040s = 46.54 MB/s
sample/reddit.json: 0.97 MB in 0.0140s = 69.36 MB/s
```

These numbers are a smoke baseline, not a final comparative claim. A release
benchmark should run more iterations and compare against the previous parser plus
cJSON, jsmn, and yyjson when those are available locally.
