#pragma once

#include <cstdint>

#include <algorithm>
#include <utility>
#include <vector>

template <typename Key>
struct IndexEntry {
    Key key{};
    uint32_t record_idx = 0;

    friend bool operator<(const IndexEntry& a, const IndexEntry& b) {
        return a.key < b.key;
    }
};

// Build a sorted index for a primitive key stored in the record.
// This keeps the key inline in the index to avoid cache misses during binary search.
template <typename Record, typename Key>
inline void build_index_member(const std::vector<Record>& records,
                               std::vector<IndexEntry<Key>>& index_out,
                               Key Record::*member) {
    index_out.clear();
    index_out.reserve(records.size());

    for (uint32_t i = 0; i < records.size(); ++i) {
        index_out.push_back(IndexEntry<Key>{records[i].*member, i});
    }

    std::sort(index_out.begin(), index_out.end(), [](const auto& a, const auto& b) {
        return a.key < b.key;
    });
}

// Returns [begin,end) positions in the index for keys in inclusive range [lo, hi].
template <typename Key>
inline std::pair<size_t, size_t> index_range(const std::vector<IndexEntry<Key>>& idx, Key lo, Key hi) {
    const auto first = std::lower_bound(idx.begin(), idx.end(), lo, [](const auto& e, const Key value) {
        return e.key < value;
    });
    const auto last = std::upper_bound(idx.begin(), idx.end(), hi, [](const Key value, const auto& e) {
        return value < e.key;
    });
    return {static_cast<size_t>(first - idx.begin()), static_cast<size_t>(last - idx.begin())};
}
