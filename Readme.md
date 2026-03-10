# Mini1 — Memory Overload: NYC 311 Service Requests

**Course:** CMPE 275 — Memory and Concurrent Processing
**Language:** C++17 | **Build:** CMake 3.16+ | **Parallelism:** OpenMP

---

## Overview

This project loads and queries a 12 GB CSV dataset (NYC 311 Service Requests, 2020–2026, ~20M records) on a 16 GB machine across three progressive implementations:

| Phase | Strategy | Avg Load | searchByZip | searchByDate | searchByBBox |
|---|---|---|---|---|---|
| Phase 1 | Serial ifstream | 93,285 ms | 3,053 ms | 2,741 ms | 2,835 ms |
| Phase 2 | Parallel OpenMP (T=4/8) | 37,286 ms | 292 ms | 60 ms | 74 ms |
| Phase 3 | SoA + mmap (T=4/8) | 36,555 ms | 20 ms | 17 ms | 41 ms |

---

## Project Structure

```
mini1/
├── common/          # Shared utilities (StringPool, StringRegistry, etc.)
├── phase1/          # Serial implementation
│   ├── include/
│   ├── src/
│   ├── benchmark/
│   └── run_benchmark.sh
├── phase2/          # Parallel implementation (OpenMP)
│   ├── include/
│   ├── src/
│   ├── benchmark/
│   └── run_benchmark.sh
├── phase3/          # SoA + mmap implementation
│   ├── include/
│   ├── src/
│   ├── benchmark/
│   └── run_benchmark.sh
├── dataset/         # Place dataset file here
│   └── nyc_311_2020_2026.csv
├── build/           # CMake build output (generated)
├── CMakeLists.txt
└── report.md
```

---

## Requirements

- macOS or Linux
- CMake 3.16+
- Clang or GCC with C++17 support
- OpenMP (`libomp` via Homebrew on macOS)
- 16 GB RAM minimum
- ~12 GB free disk space for dataset

**Install dependencies (macOS):**
```bash
brew install cmake libomp
```

---

## Dataset

Download the NYC 311 Service Requests dataset from NYC OpenData and place it at:
```
dataset/nyc_311_2020_2026.csv
```

---

## Build

```bash
# From project root
mkdir -p build && cd build
cmake ..
make -j8
```

This builds all three phases. Binaries are placed at:
```
build/phase1/phase1
build/phase2/phase2
build/phase3/phase3
```

---

## Run

### Run a single phase manually

```bash
# Phase 1
./build/phase1/phase1 dataset/nyc_311_2020_2026.csv

# Phase 2
./build/phase2/phase2 dataset/nyc_311_2020_2026.csv

# Phase 3
./build/phase3/phase3 dataset/nyc_311_2020_2026.csv
```

### Run full benchmark (10 runs + summary)

Each phase has its own benchmark script. Run from inside the phase directory:

```bash
# Phase 1
cd phase1
bash run_benchmark.sh

# Phase 2
cd phase2
bash run_benchmark.sh

# Phase 3
cd phase3
bash run_benchmark.sh
```

Results are saved to `benchmark_results.txt` inside each phase directory, with per-run timings and avg/min/max summary.

---

## Queries Benchmarked

| Query | Range |
|---|---|
| searchByZip | Zip codes 10001–10099 |
| searchByDate | Full year 2023 |
| searchByBoundingBox | Manhattan (40.57–40.74°N, 74.04–73.83°W) |

---

## Key Design Decisions

- **StringPool** — pre-allocated arena allocator replaces per-field heap allocation; eliminates 300M std::string constructions
- **StringRegistry** — low-cardinality fields interned to uint8_t/uint16_t codes; saves ~4.1 GB vs std::string
- **Epoch timestamps** — date fields parsed to uint32_t at load time; range queries are integer comparisons
- **Two StringPools** — resolution_desc alone is ~3 GB; a single pool would exceed the uint32_t offset limit
- **SoA arrays** (Phase 3) — flat zip_[], created_[], lat_[], lon_[] arrays give 100% cache line utilization vs 6.25% in AoS
- **mmap** (Phase 3) — zero-copy file mapping; CSVParser reads string_view directly from mapped buffer

---

## Notes

- Peak RAM usage is ~11.3 GB — running other memory-heavy applications concurrently may trigger OS swapping and degrade load performance significantly
- Phase 3 bounding box results differ by 186 records vs Phase 1/2 due to float vs double precision at boundary edges — expected behavior
