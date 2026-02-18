#include "parsers/CSVParser.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace {

inline std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

// Extract fields from a CSV line. Handles quoted fields and escaped quotes ("").
// Returns string_views into the input line.
static void split_csv_line(std::string_view line, std::vector<std::string_view>& out_fields) {
    out_fields.clear();

    size_t i = 0;
    while (i <= line.size()) {
        if (i == line.size()) {
            // Trailing empty field.
            out_fields.emplace_back(std::string_view{});
            break;
        }

        if (line[i] == ',') {
            out_fields.emplace_back(std::string_view{});
            ++i;
            continue;
        }

        if (line[i] == '"') {
            // Quoted field.
            ++i;
            const size_t start = i;
            bool has_escaped_quote = false;

            while (i < line.size()) {
                if (line[i] == '"') {
                    if (i + 1 < line.size() && line[i + 1] == '"') {
                        has_escaped_quote = true;
                        i += 2;
                        continue;
                    }
                    break;
                }
                ++i;
            }

            const size_t end = i;
            if (i < line.size() && line[i] == '"') {
                ++i; // consume closing quote
            }
            // Consume until comma or end.
            while (i < line.size() && line[i] != ',') {
                ++i;
            }
            if (i < line.size() && line[i] == ',') {
                ++i;
            }

            std::string_view field = line.substr(start, end - start);
            field = trim(field);

            if (!has_escaped_quote) {
                out_fields.push_back(field);
            } else {
                // Unescape into a stable string to keep correctness.
                // This path should be rare for our categorical fields.
                static thread_local std::string scratch;
                scratch.clear();
                scratch.reserve(field.size());
                for (size_t k = 0; k < field.size(); ++k) {
                    if (field[k] == '"' && k + 1 < field.size() && field[k + 1] == '"') {
                        scratch.push_back('"');
                        ++k;
                    } else {
                        scratch.push_back(field[k]);
                    }
                }
                out_fields.push_back(std::string_view(scratch));
            }
            continue;
        }

        // Unquoted field.
        const size_t start = i;
        while (i < line.size() && line[i] != ',') {
            ++i;
        }
        const size_t end = i;
        if (i < line.size() && line[i] == ',') {
            ++i;
        }
        out_fields.push_back(trim(line.substr(start, end - start)));
    }
}

inline bool parse_int64(std::string_view s, int64_t& out) {
    s = trim(s);
    if (s.empty()) {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const std::string tmp(s);
    const long long v = std::strtoll(tmp.c_str(), &end, 10);
    if (errno != 0 || end == tmp.c_str()) {
        return false;
    }
    out = static_cast<int64_t>(v);
    return true;
}

inline bool parse_int32(std::string_view s, int32_t& out) {
    int64_t v64 = 0;
    if (!parse_int64(s, v64)) {
        return false;
    }
    if (v64 < std::numeric_limits<int32_t>::min() || v64 > std::numeric_limits<int32_t>::max()) {
        return false;
    }
    out = static_cast<int32_t>(v64);
    return true;
}

inline bool parse_double(std::string_view s, double& out) {
    s = trim(s);
    if (s.empty()) {
        return false;
    }
    // strtod requires null-terminated.
    char buf[64];
    if (s.size() >= sizeof(buf)) {
        const std::string tmp(s);
        char* end = nullptr;
        out = std::strtod(tmp.c_str(), &end);
        return end != tmp.c_str();
    }
    std::memcpy(buf, s.data(), s.size());
    buf[s.size()] = '\0';
    char* end = nullptr;
    out = std::strtod(buf, &end);
    return end != buf;
}

inline bool parse_datetime_mdy_12h(std::string_view s, int64_t& out_epoch) {
    // Example: 10/05/2023 11:32:28 AM
    s = trim(s);
    if (s.empty()) {
        return false;
    }

    int month = 0, day = 0, year = 0;
    int hour = 0, minute = 0, second = 0;

    auto read_2 = [&](size_t pos, int& v) -> bool {
        if (pos + 1 >= s.size() || !std::isdigit(static_cast<unsigned char>(s[pos])) ||
            !std::isdigit(static_cast<unsigned char>(s[pos + 1]))) {
            return false;
        }
        v = (s[pos] - '0') * 10 + (s[pos + 1] - '0');
        return true;
    };

    // m/d can be 1 or 2 digits in some datasets; handle both.
    size_t i = 0;
    auto read_int = [&](int& v) -> bool {
        if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) {
            return false;
        }
        int acc = 0;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
            acc = acc * 10 + (s[i] - '0');
            ++i;
        }
        v = acc;
        return true;
    };

    if (!read_int(month)) return false;
    if (i >= s.size() || s[i] != '/') return false;
    ++i;
    if (!read_int(day)) return false;
    if (i >= s.size() || s[i] != '/') return false;
    ++i;
    if (!read_int(year)) return false;

    // skip spaces
    while (i < s.size() && s[i] == ' ') ++i;

    if (!read_int(hour)) return false;
    if (i >= s.size() || s[i] != ':') return false;
    ++i;
    if (!read_2(i, minute)) return false;
    i += 2;
    if (i >= s.size() || s[i] != ':') return false;
    ++i;
    if (!read_2(i, second)) return false;
    i += 2;

    while (i < s.size() && s[i] == ' ') ++i;
    if (i + 1 >= s.size()) return false;

    const char a = static_cast<char>(std::toupper(static_cast<unsigned char>(s[i])));
    const char b = static_cast<char>(std::toupper(static_cast<unsigned char>(s[i + 1])));
    const bool is_pm = (a == 'P' && b == 'M');
    const bool is_am = (a == 'A' && b == 'M');
    if (!is_am && !is_pm) return false;

    if (hour == 12) {
        hour = is_am ? 0 : 12;
    } else {
        hour = is_pm ? (hour + 12) : hour;
    }

    std::tm tm {};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;

#if defined(__APPLE__) || defined(__linux__)
    const time_t t = timegm(&tm);
#else
    const time_t t = std::mktime(&tm);
#endif
    if (t == static_cast<time_t>(-1)) {
        return false;
    }
    out_epoch = static_cast<int64_t>(t);
    return true;
}

inline std::string_view strip_cr(std::string_view s) {
    if (!s.empty() && s.back() == '\r') {
        s.remove_suffix(1);
    }
    return s;
}

} // namespace

void CSVParser::load_csv(const std::string& path, DataStore& out) {
    out.reset();
    out.csv_file.open_readonly(path);

    const char* data = out.csv_file.data();
    const size_t n = out.csv_file.size();
    if (data == nullptr || n == 0) {
        throw std::runtime_error("CSV file is empty or failed to map");
    }

    // Find header line.
    size_t pos = 0;
    while (pos < n && data[pos] != '\n') {
        ++pos;
    }
    const size_t header_len = (pos < n) ? pos : n;
    std::string_view header_line(data, header_len);
    header_line = strip_cr(header_line);

    std::vector<std::string_view> fields;
    fields.reserve(64);
    split_csv_line(header_line, fields);

    std::unordered_map<std::string_view, int> col;
    col.reserve(fields.size());
    for (int i = 0; i < static_cast<int>(fields.size()); ++i) {
        col.emplace(fields[i], i);
    }

    auto must = [&](std::string_view name) -> int {
        auto it = col.find(name);
        if (it == col.end()) {
            throw std::runtime_error(std::string("Missing required column: ") + std::string(name));
        }
        return it->second;
    };

    const int c_unique_key = must("Unique Key");
    const int c_created = must("Created Date");
    const int c_closed = must("Closed Date");
    const int c_due = must("Due Date");
    const int c_zip = must("Incident Zip");
    const int c_lat = must("Latitude");
    const int c_lon = must("Longitude");
    const int c_agency = must("Agency");
    const int c_borough = must("Borough");
    const int c_status = must("Status");

    // Start after header newline.
    size_t line_start = (pos < n) ? (pos + 1) : n;

    // Reserve based on known dataset size (approx). This avoids repeated reallocations.
    out.records.reserve(21'000'000);
    out.row_refs.reserve(21'000'000);

    while (line_start < n) {
        size_t line_end = line_start;
        while (line_end < n && data[line_end] != '\n') {
            ++line_end;
        }

        std::string_view line(data + line_start, line_end - line_start);
        line = strip_cr(line);

        // Skip empty lines.
        if (!line.empty()) {
            split_csv_line(line, fields);

            ServiceRequest r;
            // Unique Key
            {
                int64_t v = 0;
                if (c_unique_key < static_cast<int>(fields.size()) && parse_int64(fields[c_unique_key], v)) {
                    r.unique_key = static_cast<uint64_t>(v);
                }
            }
            // Created / Closed / Due timestamps
            if (c_created < static_cast<int>(fields.size())) {
                (void)parse_datetime_mdy_12h(fields[c_created], r.created_date);
            }
            if (c_closed < static_cast<int>(fields.size())) {
                (void)parse_datetime_mdy_12h(fields[c_closed], r.closed_date);
            }
            if (c_due < static_cast<int>(fields.size())) {
                (void)parse_datetime_mdy_12h(fields[c_due], r.due_date);
            }
            // Zip
            if (c_zip < static_cast<int>(fields.size())) {
                (void)parse_int32(fields[c_zip], r.zip);
            }
            // Lat/Lon (optional now; stored for future)
            {
                double v = std::numeric_limits<double>::quiet_NaN();
                if (c_lat < static_cast<int>(fields.size()) && parse_double(fields[c_lat], v)) {
                    r.latitude = v;
                } else {
                    r.latitude = std::numeric_limits<double>::quiet_NaN();
                }
                if (c_lon < static_cast<int>(fields.size()) && parse_double(fields[c_lon], v)) {
                    r.longitude = v;
                } else {
                    r.longitude = std::numeric_limits<double>::quiet_NaN();
                }
            }
            // Flyweight categorical IDs
            if (c_agency < static_cast<int>(fields.size())) {
                r.agency_id = out.agency_pool.intern(fields[c_agency]);
            }
            if (c_borough < static_cast<int>(fields.size())) {
                r.borough_id = out.borough_pool.intern(fields[c_borough]);
            }
            if (c_status < static_cast<int>(fields.size())) {
                r.status_id = out.status_pool.intern(fields[c_status]);
            }

            out.records.push_back(r);
            out.row_refs.push_back(RowRef{static_cast<uint64_t>(line_start), static_cast<uint32_t>(line.size())});
        }

        line_start = (line_end < n) ? (line_end + 1) : n;
    }
}
