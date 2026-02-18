#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "models/ServiceRequest.h"
#include "storage/Index.h"
#include "storage/MMapFile.h"
#include "storage/StringPool.h"

struct RowRef {
    uint64_t offset = 0;
    uint32_t length = 0;
};

class DataStore {
public:
    // Cold-column backing store (all columns accessible): mmap + row references.
    MMapFile csv_file;
    std::vector<RowRef> row_refs;

    std::vector<ServiceRequest> records;

    // Cache-friendly indices: key is stored in index entry (no pointer chase during binary search)
    std::vector<IndexEntry<int64_t>> idx_created_date;
    std::vector<IndexEntry<int64_t>> idx_closed_date;
    std::vector<IndexEntry<int32_t>> idx_zip;

    StringPool agency_pool;
    StringPool borough_pool;
    StringPool status_pool;

    std::string_view row_view(uint32_t record_idx) const {
        const RowRef r = row_refs.at(record_idx);
        return std::string_view(csv_file.data() + r.offset, r.length);
    }

    void clear() {
        records.clear();
        row_refs.clear();
        idx_created_date.clear();
        idx_closed_date.clear();
        idx_zip.clear();
        // Pools intentionally not cleared here yet; decide based on lifecycle.
    }

    void reset() {
        clear();
        agency_pool.clear();
        borough_pool.clear();
        status_pool.clear();
        csv_file.close();
    }
};
