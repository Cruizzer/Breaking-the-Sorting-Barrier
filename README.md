# Breaking-the-Sorting-Barrier

Single-source shortest paths benchmark project comparing binary-heap Dijkstra, Fibonacci-heap Dijkstra, and BMSSP. The repository includes the implementation, unit tests, benchmark drivers, scale benchmarking, and the plotting scripts used to generate the figures in [finalReportTemplateLaTeX/figures](finalReportTemplateLaTeX/figures).

This README is the main entry point for build, test, benchmark, and figure-generation workflows.

## Project Layout

- [src/](src) - executable entry points, benchmark drivers, and algorithm implementations
- [include/](include) - public headers
- [test/](test) - Catch2 unit tests
- [benchmarks_google/](benchmarks_google) - Google Benchmark microbenchmarks
- [tools/](tools) - plot generation and reproducibility helpers
- [finalReportTemplateLaTeX/](finalReportTemplateLaTeX) - report sources and generated figures

## Build

Requirements:

- CMake 3.15 or newer
- A C++17 compiler
- Internet access on first configure so CMake can fetch Catch2 and Google Benchmark

Configure and build from the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Key targets:

- `sssp` - main comparison executable
- `unit_tests` - Catch2 test binary
- `gbench` - Google Benchmark microbenchmarks
- `scale_bench` - scale benchmark driver that writes the CSV used for analysis

## Main Executable

The `sssp` binary compares BMSSP against a selected Dijkstra variant on a generated graph.

Usage:

```bash
./build/sssp --generate TYPE SIZE [OPTIONS]
```

Supported graph types:

- `random` - Erdős-Rényi style random graph
- `er` - alias for `random`
- `ba` - Barabási-Albert graph
- `grid` - rectangular grid graph
- `road` - road-like graph

Options:

- `--trials K` - run multiple sources
- `--report FILE` - write a CSV report
- `--dijkstra binary|fib` - choose the Dijkstra variant

Examples:

```bash
./build/sssp --generate er 10000 4
./build/sssp --generate ba 10000 3
./build/sssp --generate grid 100 100
./build/sssp --generate road 50000
./build/sssp --generate er 100000 8 --dijkstra fib
./build/sssp --generate er 26000 12 --dijkstra binary --report run_binary.csv
```

## Unit Tests

Run all Catch2 tests:

```bash
./build/unit_tests
```

Filter tests by tag:

```bash
./build/unit_tests [dijkstra]
./build/unit_tests [bmssp]
./build/unit_tests [graph]
```

List discovered tests and tags:

```bash
./build/unit_tests --list-tests
./build/unit_tests --list-tags
```

Run via CTest:

```bash
ctest --test-dir build
ctest --test-dir build -V
ctest --test-dir build -R dijkstra
```

## Google Benchmark

Run the microbenchmarks:

```bash
./build/gbench
```

Useful filters and options:

```bash
./build/gbench --benchmark_filter=Random
./build/gbench --benchmark_filter=Grid
./build/gbench --benchmark_filter=Dijkstra
./build/gbench --benchmark_filter=BMSSP
./build/gbench --benchmark_repetitions=10
./build/gbench --benchmark_min_time=2s
./build/gbench --benchmark_out=benchmark_results.json --benchmark_out_format=json
./build/gbench --benchmark_out=benchmark_results.csv --benchmark_out_format=csv
```

## Scale Benchmark

The scale benchmark is the main driver for the chapter 4 figures. It writes `scale_results.csv` in the repository root.

Run the default profile:

```bash
./build/scale_bench
```

Useful options:

- `--scale baseline|large|xlarge`
- `--sizes N1,N2,...`
- `--trials N`
- `--warmup N`

Examples:

```bash
./build/scale_bench --scale large
./build/scale_bench --scale xlarge
./build/scale_bench --sizes 100000,250000,500000,1000000 --trials 5 --warmup 1
```

The generated CSV includes:

- graph family, size, and generator parameters
- binary-heap Dijkstra timings
- Fibonacci-heap Dijkstra timings
- BMSSP timings
- median, standard deviation, and p95 timing columns
- correctness match counters
- graph-structure metrics
- operation telemetry for binary heap, Fibonacci heap, and BMSSP
- BMSSP preprocessing metrics

## Plot Generation

The report figures are generated from the scale CSV with the scripts in [tools/](tools).

Generate the current figure set:

```bash
python3 tools/plot_scale_new_constraints.py --csv scale_results.csv --outdir finalReportTemplateLaTeX/figures
python3 tools/generate_claimed_figures.py --csv scale_results.csv --outdir finalReportTemplateLaTeX/figures
```

Expected outputs include:

- [finalReportTemplateLaTeX/figures/runtime_median_family_grid.png](finalReportTemplateLaTeX/figures/runtime_median_family_grid.png)
- [finalReportTemplateLaTeX/figures/variability_summary.png](finalReportTemplateLaTeX/figures/variability_summary.png)
- [finalReportTemplateLaTeX/figures/bmssp_prep_impact_ratio.png](finalReportTemplateLaTeX/figures/bmssp_prep_impact_ratio.png)
- [finalReportTemplateLaTeX/figures/speedup_ratio_heatmaps.png](finalReportTemplateLaTeX/figures/speedup_ratio_heatmaps.png)
- [finalReportTemplateLaTeX/figures/pq_telemetry.png](finalReportTemplateLaTeX/figures/pq_telemetry.png)
- [finalReportTemplateLaTeX/figures/stale_vs_fib_gap.png](finalReportTemplateLaTeX/figures/stale_vs_fib_gap.png)
- [finalReportTemplateLaTeX/figures/c2c1_stability.png](finalReportTemplateLaTeX/figures/c2c1_stability.png)
- [finalReportTemplateLaTeX/figures/structural_correlates.png](finalReportTemplateLaTeX/figures/structural_correlates.png)

## BMSSP Usage

The BMSSP implementation is exposed through the algorithm headers under [include/algorithms/](include/algorithms). Typical usage:

```cpp
#include "algorithms/bmssp.hpp"

duan25::Solver solver(graph.size());
solver.prepare_graph(false);
auto [distances, predecessors] = solver.execute(source);
auto path = solver.reconstruct_path(target, predecessors);
```

The wrapper functions in the `algorithms` namespace are also available:

```cpp
auto distances = algorithms::bmssp(graph, source);
auto distance = algorithms::bmssp_single_target(graph, source, target);
auto path = algorithms::bmssp_path(graph, source, target);
```

## Custom Targets

From the build directory:

```bash
cmake --build . --target test_run
cmake --build . --target benchmark_run
cmake --build . --target test_all
```

Or from the repository root:

```bash
cmake --build build --target test_run
cmake --build build --target benchmark_run
cmake --build build --target test_all
```

## Quick Maintenance

```bash
git status
cmake --build build --clean-first -j
rm -rf build && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
```

## Notes

- The current final-report figures are generated into [finalReportTemplateLaTeX/figures](finalReportTemplateLaTeX/figures).
- Older helper markdown files in the repository are archival and may contain stale commands or historical notes.