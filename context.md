# Project Context (Living Log) — CMPE275 mini1

This file is a **living context log** (not finalized). I will reference it whenever you ask something and update it after each answer.

## Current Dataset
- File: `dataset/nyc_311_2020_2026.csv`
- Size/scale (observed earlier): ~12 GB, ~20.1M rows
- Topic: NYC 311 service requests (allowed; not collisions/violations)

## Dataset Columns (Extracted Header)
Column count: 44
1. Unique Key
2. Created Date
3. Closed Date
4. Agency
5. Agency Name
6. Problem (formerly Complaint Type)
7. Problem Detail (formerly Descriptor)
8. Additional Details
9. Location Type
10. Incident Zip
11. Incident Address
12. Street Name
13. Cross Street 1
14. Cross Street 2
15. Intersection Street 1
16. Intersection Street 2
17. Address Type
18. City
19. Landmark
20. Facility Type
21. Status
22. Due Date
23. Resolution Description
24. Resolution Action Updated Date
25. Community Board
26. Council District
27. Police Precinct
28. BBL
29. Borough
30. X Coordinate (State Plane)
31. Y Coordinate (State Plane)
32. Open Data Channel Type
33. Park Facility Name
34. Park Borough
35. Vehicle Type
36. Taxi Company Borough
37. Taxi Pick Up Location
38. Bridge Highway Name
39. Bridge Highway Direction
40. Road Ramp
41. Bridge Highway Segment
42. Latitude
43. Longitude
44. Location

## Project Requirements (from mini1 doc)
- Phase 1: **Serial** C/C++ library, OOP design (classes), parse CSV, store fields as primitive types, provide APIs for reading + basic range searching, benchmark 10+ runs.
- Phase 2: Parallelize (OpenMP/threads) and compare.
- Phase 3: Optimize (Object-of-Arrays) and compare.
- No DBs/services; in-memory only. CMake required. Boost optional.

## Design Direction (Phase 1)
- OOP is for the **library architecture** (Facade/Repository/Parser/etc.).
- Records should be stored as compact **value types** in contiguous containers (e.g., `std::vector<Record>`), not heap-per-row objects.
- Memory strategy: Flyweight/string interning for repeated categorical strings; avoid keeping large free-text columns in the per-record struct unless needed.
- Query strategy: cache-friendly sorted indices using `IndexEntry{key, record_idx}` for range queries; partition/ranges for low-cardinality categorical filters.

### Requirement update: “Load all columns in memory”
- Team decision: support **access to all 44 columns**.
- Constraint: dataset is ~12GB and machine RAM is 16GB; fully materializing all string columns naively (per-row `std::string`) is likely to exceed RAM.
- Planned solution: **two-tier storage**
    1. **Hot columns (materialized):** store query-critical fields as primitives + compact IDs (timestamps, zip, lat/lon, categorical IDs).
    2. **Cold columns (backing store):** keep the raw CSV file **memory-mapped** and store a `RowRef` (byte offset + length) per record; when a cold column is requested, parse that row slice lazily to extract the requested field.
         - This supports “all columns are available” while avoiding duplicating large text in RAM.
         - Range queries remain fast because they operate only on hot columns + indices.

## How We Store Each Record (Phase 1)

### Primary storage
- `std::vector<ServiceRequest> records;` (contiguous, cache/prefetch friendly)
- `ServiceRequest` is a compact, POD-like struct of **primitive types**.

### Cold-column backing store (all columns accessible)
- Maintain a memory-mapped view of the CSV file.
- Store per-record `RowRef { uint64_t offset; uint32_t length; }` pointing to the raw CSV line.
- Cold string columns are extracted on demand from the mapped row (lazy parse).

### Missing/invalid values
- Use **sentinel values** instead of `std::optional` per field:
    - missing timestamps: `-1`
    - missing zip: `-1`
    - missing lat/lon: prefer `NaN` (or keep `0.0` only if we can guarantee it’s not ambiguous)

### Strings / categorical fields
- Do **not** store per-row `std::string`.
- Store small integer IDs in the record (`uint16_t agency_id`, `borough_id`, `status_id`).
- Map string↔id using `StringPool` (flyweight interning).

### Large free-text columns
- Phase 1 range queries do not require keeping huge text columns (e.g., long descriptions).
- Default approach: **do not store** those fields in `ServiceRequest` to keep memory bounded.

## Planned Phase-1 Folder Structure
```
.
├── CMakeLists.txt
├── context.md
├── dataset/
│   └── nyc_311_2020_2026.csv
└── src/
    ├── api/
    │   └── NYC311Service.h
    ├── models/
    │   └── ServiceRequest.h
    ├── parsers/
    │   └── CSVParser.h
    ├── storage/
    │   ├── DataStore.h
    │   ├── Index.h
    │   └── StringPool.h
    ├── benchmark/
    │   └── Benchmark.h
    └── main.cpp
```

## Phase-1 “Basic Range Queries” (initial shortlist)
- Phase-1 initial supported range queries:
    - Created Date range
    - Closed Date range
    - Incident Zip range

(Planned next additions: Latitude/Longitude ranges + bounding box; Date range + Borough/Agency filter)

## Modularity Plan (Adding More Filter Attributes Later)
- Indices are built via a generic helper: `build_index_member(records, index, &Record::field)`.
- Range queries use a generic helper over `IndexEntry{key, record_idx}`.
- Adding a new range-query attribute should be: add field → add index vector → 1-line build in `build_indices()` → thin query wrapper.

## End-to-End Flow (Phase 1)

### 0) Build + run
- Configure/build with CMake (serial baseline):
    - `cmake -S . -B build`
    - `cmake --build build -j`
- Run the executable: `./build/mini1`

### 1) Library entry point (Facade)
- User code (benchmark harness / main) interacts only with the Facade: `NYC311Service`.
- Facade owns:
    - `CSVParser` (ingestion)
    - `DataStore` (records + indices + pools)

### 2) Load phase (serial)
- Call: `NYC311Service::load_csv(path)`
- Implementation: `CSVParser::load_csv(path, DataStore&)`
- Outcomes:
    1. Parse the header to map column names → indices.
    2. Read each CSV row and build one `ServiceRequest` (hot fields only) and append to `DataStore.records`.
    3. For categorical fields needed in filters (Agency/Borough/Status), intern strings into `StringPool` and store IDs in the record.
    4. For missing/invalid values, store sentinels (`-1` timestamps/zip; `NaN` for missing lat/lon if used).

### 3) “All columns accessible” requirement (two-tier storage)
- Hot tier: materialized primitives + IDs in `ServiceRequest`.
- Cold tier: memory-map the raw CSV file and store per-row `RowRef{offset,length}`.
- When a cold string column is requested later, parse only that row slice and extract the requested field.

### 4) Index build phase (serial)
- Call: `NYC311Service::build_indices()`
- Uses generic helper `build_index_member(records, index, &ServiceRequest::field)`.
- Builds cache-friendly sorted indices:
    - Created Date index: `IndexEntry<int64_t>{key=created_date, record_idx}`
    - Closed Date index: `IndexEntry<int64_t>{key=closed_date, record_idx}`
    - Zip index: `IndexEntry<int32_t>{key=zip, record_idx}`

### 5) Query execution phase
- Query calls run over the relevant index:
    1. `lower_bound/upper_bound` to find the matching slice `[begin,end)` in the index.
    2. Compute `count = end-begin`.
    3. If `count_only=true`, return just the count (no allocations).
    4. Otherwise, return `record_indices` (uint32 indices) up to `limit`.

### 6) Why this is hardware-friendly
- Binary search comparisons touch only the index array (key stored inline).
- Scanning `[begin,end)` is sequential → good prefetch/cache behavior.
- Returning indices avoids copying full records or strings.

### 7) Extension points (adding more attributes)
- Add a new hot field to `ServiceRequest` (primitive type).
- Add a new `idx_*` vector to `DataStore`.
- Add one line in `build_indices()`.
- Add one thin query wrapper that calls the generic range helper.

## Additional Range-Query Candidates (From Dataset Columns)

These columns are also naturally range-searchable (numeric or datetime) and can be added if we want more variety for benchmarking:

### Datetime range
- Due Date
- Resolution Action Updated Date

### Numeric/integer range
- X Coordinate (State Plane)
- Y Coordinate (State Plane)
- Council District (small integer range)
- Police Precinct (small integer range)
- BBL (large integer; can be used for range but may have many missing values)
- Unique Key (good for testing indexing but less meaningful analytically)

### Not good for “range” (mostly strings)
- Community Board looks like a string (e.g., "07 BRONX"), so treat as categorical unless we parse it.

## Next Implementation Step
- Scaffold directories + minimal headers + CMake so we can compile a skeleton binary before filling in parsing/index/query logic.

## Current Status
- Phase-1 scaffold created: `src/` layout + headers + `CMakeLists.txt`.
- CMake installed (verified `cmake version 4.2.3`).
- Skeleton builds successfully with AppleClang 17 via:
    - `cmake -S . -B build`
    - `cmake --build build -j`

## Implementation Status
- Load phase is now implemented in: src/parsers/CSVParser.cpp
    - Uses `mmap` (`MMapFile`) for cold backing store
    - Stores `RowRef{offset,length}` per record for later cold-column access
    - Materializes hot columns into `std::vector<ServiceRequest>` (Unique Key, Created/Closed/Due, Zip, Lat/Lon, Agency/Borough/Status IDs)

## Load Phase (Detailed Flow + Assumptions)

### Detailed flow (what the loader actually does)
1. **Reset store**: `DataStore::reset()` clears vectors + pools and closes any previous mapping.
2. **Memory-map the CSV**: `MMapFile::open_readonly(path)` maps the entire file into the process address space (`const char* data`, `size_t size`).
3. **Read header line**:
    - Scan until first `\n` to get the header line.
    - Split header into columns via `split_csv_line()`.
    - Build a map: `column_name -> index`.
4. **Resolve required columns**:
    - `must("Unique Key")`, `must("Created Date")`, `must("Closed Date")`, `must("Due Date")`, `must("Incident Zip")`, `must("Latitude")`, `must("Longitude")`, `must("Agency")`, `must("Borough")`, `must("Status")`.
5. **Reserve capacity**:
    - Pre-reserve ~21M for `records` and `row_refs` to reduce reallocations.
6. **Scan lines (rows)**:
    - For each line (between newlines):
      - Create `std::string_view line` into the mapped memory.
      - Split row into fields (`split_csv_line`).
      - Build a `ServiceRequest` with parsed hot fields:
         - `Unique Key` -> `uint64_t`
         - `Created/Closed/Due` -> `int64_t` epoch (UTC via `timegm` on macOS)
         - `Incident Zip` -> `int32_t`
         - `Latitude/Longitude` -> `double` (missing => `NaN`)
         - `Agency/Borough/Status` -> interned IDs via `StringPool`
      - Append record to `records`.
      - Append `RowRef{offset,length}` to `row_refs` so we can retrieve the raw row later for cold columns.

### Assumptions / current limitations
- **Row delimiter**: loader treats newline (`\n`) as row boundary. CSV rows containing embedded newlines inside quotes are not handled.
- **Escaped quotes**: `split_csv_line` supports doubled quotes (`""`). It unescapes via a thread-local scratch string (correctness > speed in that rare case).
- **Datetime format**: expects `M/D/YYYY HH:MM:SS AM|PM`. Invalid/missing values remain sentinel `-1`.
- **Zip parsing**: expects numeric zip; invalid/missing stays `-1`.
