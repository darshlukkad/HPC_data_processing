#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <shared_mutex>

template<typename T>
class StringRegistry {
    public:
        StringRegistry() {
            encode_map_[""] = static_cast<T>(0);
            decode_vec_.push_back("");
        }

        T encode(std::string_view sv) {
            {
                std::shared_lock lk(mu_);
                auto it = encode_map_.find(std::string(sv));
                if (it != encode_map_.end()) return it->second;
            }
            std::unique_lock lk(mu_);
            // double-check after acquiring write lock
            std::string s(sv);
            auto it = encode_map_.find(s);
            if (it != encode_map_.end()) return it->second;
            T id = static_cast<T>(decode_vec_.size());
            encode_map_[s] = id;
            decode_vec_.push_back(s);
            return id;
        }

        const std::string& decode(T id) const {
            std::shared_lock lk(mu_);
            return decode_vec_[static_cast<size_t>(id)];
        }

        T size() const {
            return static_cast<T>(decode_vec_.size());
        }
    
    private:
        std::unordered_map<std::string, T> encode_map_;
        std::vector<std::string> decode_vec_;
        mutable std::shared_mutex mu_;
};
