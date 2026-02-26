# CMPE 275 Mini 1 — NYC 311 (Phase 1)

C++20 / CMake project for CMPE 275 Mini 1 using the NYC 311 Service Requests (2020–2026) CSV dataset.

This repo currently contains a **Phase 1 skeleton**:
- Serial CSV loader backed by **memory-mapped** storage (keeps the full CSV accessible without copying every string).
- A compact in-memory record struct for “hot” fields.
- Cache-friendly sorted indices + range query helpers for:
  - Created Date (epoch seconds)
  - Closed Date (epoch seconds)
  - Incident Zip

## Dataset

Place the dataset at:
- `dataset/nyc_311_2020_2026.csv`

Notes:
- The dataset is intentionally **not** committed to git (see `.gitignore`).

## Build

Prereqs:
- CMake
- A C++20 compiler (AppleClang on macOS is fine)

Commands:

```bash
cmake -S . -B build
cmake --build build -j
```

## Run

Right now `src/main.cpp` is only a compilation / wiring check (it does not yet load the dataset or run benchmarks):

```bash
./build/mini1
```

## Library Usage (current API)

The main entry point is `NYC311Service`.

Example (add something like this to `src/main.cpp` when you’re ready):

```cpp
#include "api/NYC311Service.h"

int main() {
    NYC311Service svc;
    svc.load_csv("dataset/nyc_311_2020_2026.csv");
    svc.build_indices();

    // Query: created date range (epoch seconds)
    QueryOptions opt;
    opt.count_only = true;

    const auto res = svc.query_created_date(Range<int64_t>{1704067200, 1706745599}, opt);
    // res.count holds the number of matching records
}
```

Queries return a `QueryResult`:
- `count`: number of matches
- `record_indices`: optional list of matching row indices (suppressed when `count_only=true`)

## “All Columns In Memory” approach

This code uses a two-tier model:
- **Hot columns** are parsed into `ServiceRequest` (numeric timestamps, zip, etc.).
- **Cold columns** remain accessible via the memory-mapped CSV + per-row `(offset,length)` stored in `DataStore::row_refs`.

You can access the raw CSV line for a record via:
- `DataStore::row_view(record_idx)` (currently internal to the service)

## Repo layout

- `src/api/NYC311Service.h`: facade API, index build, range query wrappers
- `src/parsers/CSVParser.*`: serial mmap-based CSV loader
- `src/storage/*`: `DataStore`, `MMapFile`, `Index`, `StringPool`
- `src/models/ServiceRequest.h`: compact hot record type

## Limitations (known)

- CSV parsing assumes each record is contained on a single newline.
  - If the dataset contains **embedded newlines inside quoted fields**, the loader will need to be upgraded.
- Date parsing expects the dataset format like `M/D/YYYY HH:MM:SS AM/PM`.

