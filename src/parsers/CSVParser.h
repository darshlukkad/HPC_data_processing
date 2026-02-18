#pragma once

#include <string>

#include "storage/DataStore.h"

// Phase 1: serial CSV parsing into DataStore.
class CSVParser {
public:
    void load_csv(const std::string& path, DataStore& out);
};
