#include "StringPool.hpp"
#include <cstring>
#include <iostream>
#include <cstdlib>

StringPool::StringPool(uint32_t capacity_bytes)
    : buffer_(new char[capacity_bytes])
    , capacity_(capacity_bytes)
    , top_(0)
    {}

StringPool::~StringPool()
{
    delete[] buffer_;
}

StringRef StringPool::store(const char* str, uint16_t len)
{
    uint32_t offset = top_.fetch_add(len, std::memory_order_relaxed);
    if (offset + len > capacity_) {
        std::cerr << "StringPool overflow: used=" << offset
        << " capacity=" << capacity_ << "\n";
        std::abort();
    }
    std::memcpy(buffer_ + offset, str, len);
    return {offset, len};

}

const char* StringPool::get(StringRef ref) const
{
    return buffer_ + ref.offset;
}

uint32_t StringPool::used() const {
    return top_.load();
}

uint32_t StringPool::capacity() const {
    return capacity_;
}