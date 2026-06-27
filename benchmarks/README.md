# Benchmark Notes

The benchmark harness is intentionally small and portable. It parses the bundled
sample corpus repeatedly and reports throughput in MB/s:

```sh
cmake -S . -B build
cmake --build build
./build/csjp_bench 1000
```

The output is a smoke signal for local changes, not a portable performance
claim.
