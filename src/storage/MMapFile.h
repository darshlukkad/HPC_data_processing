#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

class MMapFile {
public:
    MMapFile() = default;
    ~MMapFile() { close(); }

    MMapFile(const MMapFile&) = delete;
    MMapFile& operator=(const MMapFile&) = delete;

    MMapFile(MMapFile&& other) noexcept { move_from(other); }
    MMapFile& operator=(MMapFile&& other) noexcept {
        if (this != &other) {
            close();
            move_from(other);
        }
        return *this;
    }

    void open_readonly(const std::string& path) {
        close();

        fd_ = ::open(path.c_str(), O_RDONLY);
        if (fd_ < 0) {
            throw std::runtime_error("Failed to open file: " + path);
        }

        struct stat st {};
        if (::fstat(fd_, &st) != 0) {
            ::close(fd_);
            fd_ = -1;
            throw std::runtime_error("Failed to stat file: " + path);
        }

        size_ = static_cast<size_t>(st.st_size);
        if (size_ == 0) {
            // Allow empty mapping, but keep ptr null.
            return;
        }

        void* p = ::mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
        if (p == MAP_FAILED) {
            ::close(fd_);
            fd_ = -1;
            size_ = 0;
            throw std::runtime_error("mmap failed for file: " + path);
        }

        data_ = static_cast<const char*>(p);
    }

    void close() {
        if (data_ != nullptr && size_ > 0) {
            ::munmap(const_cast<char*>(data_), size_);
        }
        data_ = nullptr;
        size_ = 0;

        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    const char* data() const { return data_; }
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

private:
    int fd_ = -1;
    const char* data_ = nullptr;
    size_t size_ = 0;

    void move_from(MMapFile& other) {
        fd_ = other.fd_;
        data_ = other.data_;
        size_ = other.size_;
        other.fd_ = -1;
        other.data_ = nullptr;
        other.size_ = 0;
    }
};
