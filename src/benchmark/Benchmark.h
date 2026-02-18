#pragma once

#include <chrono>
#include <cstdint>

class Benchmark {
public:
    template <typename Fn>
    static uint64_t time_us(Fn&& fn) {
        const auto start = std::chrono::steady_clock::now();
        fn();
        const auto end = std::chrono::steady_clock::now();
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
    }
};
