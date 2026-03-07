#pragma once
#include "../include/ParallelDataStore.hpp"
#include <string>
#include <cstdint>

struct BenchmarkResult {
    std::string name;
    double elapsed_ms;
    size_t result_count;
    int num_threads;
};

struct Benchmark {
    static BenchmarkResult measureLoad(ParallelDataStore& ds,
                                         const std::string& path, int num_threads);

    static BenchmarkResult measureSearchByZip(const ParallelDataStore& ds,
                                              uint32_t zip_min, uint32_t zip_max, int num_threads);

    static BenchmarkResult measureSearchByDate(const ParallelDataStore& ds,
                                               uint32_t from, uint32_t to, int num_threads);

    static BenchmarkResult measureSearchByBoundingBox(const ParallelDataStore& ds,
                                                      double lat_min, double lat_max,
                                                      double lon_min, double lon_max,
                                                      int num_threads);
    };