#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "parsers/CSVParser.h"
#include "storage/DataStore.h"
#include "storage/Index.h"

template <typename T>
struct Range {
    T lo;
    T hi;
};

struct QueryOptions {
    bool count_only = false;
    uint32_t limit = 0; // 0 = no limit
};

struct QueryResult {
    uint64_t count = 0;
    std::vector<uint32_t> record_indices;
};

class NYC311Service {
public:
    void load_csv(const std::string& path) { parser_.load_csv(path, store_); }
    void build_indices() {
        build_index_member(store_.records, store_.idx_created_date, &ServiceRequest::created_date);
        build_index_member(store_.records, store_.idx_closed_date, &ServiceRequest::closed_date);
        build_index_member(store_.records, store_.idx_zip, &ServiceRequest::zip);
    }
    void clear() { store_.clear(); }

    uint64_t record_count() const { return static_cast<uint64_t>(store_.records.size()); }

    QueryResult query_created_date(Range<int64_t> r, QueryOptions opt = {}) const;
    QueryResult query_closed_date(Range<int64_t> r, QueryOptions opt = {}) const;
    QueryResult query_zip(Range<int32_t> r, QueryOptions opt = {}) const;

private:
    DataStore store_;
    CSVParser parser_;
};

namespace detail {
template <typename Key>
inline QueryResult range_query_impl(const std::vector<IndexEntry<Key>>& idx, Range<Key> r, QueryOptions opt) {
    QueryResult out;
    if (idx.empty()) {
        return out;
    }
    if (r.hi < r.lo) {
        return out;
    }

    const auto [begin_pos, end_pos] = index_range(idx, r.lo, r.hi);
    const uint64_t count = static_cast<uint64_t>(end_pos - begin_pos);
    out.count = count;

    if (opt.count_only || count == 0) {
        return out;
    }

    const uint64_t take = (opt.limit == 0) ? count : std::min<uint64_t>(count, opt.limit);
    out.record_indices.reserve(static_cast<size_t>(take));
    for (size_t i = begin_pos; i < end_pos && out.record_indices.size() < take; ++i) {
        out.record_indices.push_back(idx[i].record_idx);
    }
    return out;
}
} // namespace detail

inline QueryResult NYC311Service::query_created_date(Range<int64_t> r, QueryOptions opt) const {
    return detail::range_query_impl(store_.idx_created_date, r, opt);
}

inline QueryResult NYC311Service::query_closed_date(Range<int64_t> r, QueryOptions opt) const {
    return detail::range_query_impl(store_.idx_closed_date, r, opt);
}

inline QueryResult NYC311Service::query_zip(Range<int32_t> r, QueryOptions opt) const {
    return detail::range_query_impl(store_.idx_zip, r, opt);
}
