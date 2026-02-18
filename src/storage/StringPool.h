#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Flyweight interning for repeated categorical strings.
// Phase 1: keep minimal; we can optimize hashing/storage later.
class StringPool {
public:
    using Id = uint16_t;

    struct TransparentHash {
        using is_transparent = void;
        size_t operator()(std::string_view sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }
        size_t operator()(const std::string& s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }
    };

    struct TransparentEq {
        using is_transparent = void;
        bool operator()(const std::string& a, const std::string& b) const noexcept { return a == b; }
        bool operator()(const std::string& a, std::string_view b) const noexcept { return a == b; }
        bool operator()(std::string_view a, const std::string& b) const noexcept { return a == b; }
    };

    Id intern(std::string_view value) {
        auto it = lookup_.find(value);
        if (it != lookup_.end()) {
            return it->second;
        }
        const Id id = static_cast<Id>(values_.size());
        values_.push_back(std::string(value));
        lookup_.emplace(values_.back(), id);
        return id;
    }

    std::string_view get(Id id) const {
        return values_.at(id);
    }

    size_t size() const { return values_.size(); }

    void clear() {
        values_.clear();
        lookup_.clear();
    }

private:
    std::vector<std::string> values_;
    std::unordered_map<std::string, Id, TransparentHash, TransparentEq> lookup_;
};
