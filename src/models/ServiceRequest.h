#pragma once

#include <cstdint>

// Phase 1: compact value type (not heap-per-row object)
// Strings are expected to be flyweight IDs (interned) rather than std::string.
struct ServiceRequest {
    uint64_t unique_key = 0;

    int64_t created_date = -1; // unix timestamp, -1 for missing
    int64_t closed_date = -1;  // unix timestamp, -1 for missing
    int64_t due_date = -1;     // unix timestamp, -1 for missing

    int32_t zip = -1;          // -1 for missing/invalid

    double latitude = 0.0;     // consider NaN as missing if needed
    double longitude = 0.0;    // consider NaN as missing if needed

    uint16_t agency_id = 0;
    uint16_t borough_id = 0;
    uint16_t status_id = 0;
};
