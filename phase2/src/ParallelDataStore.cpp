#include "ParallelDataStore.hpp"
#include "DateParser.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <omp.h>

static const int MAX_FIELDS = 50;
static const uint32_t POOL_BYTES = 3500u * 1024u * 1024u;  // 3.5 GB, fits in uint32_t
static const uint32_t RES_POOL_BYTES = 3200u * 1024u * 1024u; // 3.2 GB for resolution_desc

ParallelDataStore::ParallelDataStore(): pool_(POOL_BYTES), res_pool_(RES_POOL_BYTES) {}

static uint32_t parseUint32(std::string_view sv) {
    if (sv.empty()) return 0;
    char buf[32];
    size_t n = std::min(sv.size(), sizeof(buf)-1);
    std::memcpy(buf, sv.data(), n);
    buf[n] = '\0';
    return (uint32_t)std::strtoul(buf, nullptr, 10);
}

static uint64_t parseUint64(std::string_view sv) {
    if (sv.empty()) return 0;
    char buf[32];
    size_t n = std::min(sv.size(), sizeof(buf)-1);
    std::memcpy(buf, sv.data(), n);
    buf[n] = '\0';
    return (uint64_t)std::strtoull(buf, nullptr, 10);
}

static double parseDouble(std::string_view sv) {
    if (sv.empty()) return 0.0;
    char buf[64];
    size_t n = std::min(sv.size(), sizeof(buf)-1);
    std::memcpy(buf, sv.data(), n);
    buf[n] = '\0';
    return std::strtod(buf, nullptr);
}

static uint32_t parseDateField(std::string_view sv) {
    if (sv.empty()) return 0;
    return parseDateTime(sv.data(), (int)sv.size());
}

ServiceRequest ParallelDataStore::parseRow(std::string_view* f, int n) {
    auto field = [&](int i) -> std::string_view {
        return (i<n) ? f[i] : std::string_view{};
    };

    auto storeStr = [&](int i) -> StringRef {
        auto sv = field(i);
        if (sv.empty()) return {0, 0};
        return pool_.store(sv.data(), (uint16_t)(sv.size()));
    };

    ServiceRequest r{};
    r.unique_key                 = parseUint64(field(0));
    r.time.created               = parseDateField(field(1));
    r.time.closed                = parseDateField(field(2));
    r.service.agency             = agency_reg_.encode(field(3));
    r.service.agency_name        = storeStr(4);
    r.service.problem            = problem_reg_.encode(field(5));
    r.service.problem_detail     = problem_detail_reg_.encode(field(6));
    r.service.additional_details = storeStr(7);
    r.service.location_type      = location_type_reg_.encode(field(8));
    r.location.zip               = parseUint32(field(9));
    r.location.address           = storeStr(10);
    r.location.street_name       = storeStr(11);
    r.location.cross_street_1    = storeStr(12);
    r.location.cross_street_2    = storeStr(13);
    r.location.intersection_1    = storeStr(14);
    r.location.intersection_2    = storeStr(15);
    r.location.address_type      = address_type_reg_.encode(field(16));
    r.location.city              = storeStr(17);
    r.location.landmark          = storeStr(18);
    r.service.facility_type      = storeStr(19);
    r.service.status             = status_reg_.encode(field(20));
    r.time.due                   = parseDateField(field(21));
    {
        auto sv = field(22);
        if (!sv.empty())
            r.service.resolution_desc = res_pool_.store(sv.data(), (uint16_t)sv.size());
    }
    r.time.resolution            = parseDateField(field(23));
    r.admin.community_board      = (uint16_t)parseUint32(field(24));
    r.admin.council_district     = (uint16_t)parseUint32(field(25));
    r.admin.police_precinct      = (uint16_t)parseUint32(field(26));
    r.admin.bbl                  = parseUint64(field(27));
    r.location.borough           = borough_reg_.encode(field(28));
    r.location.x_coord           = (int32_t)parseUint32(field(29));
    r.location.y_coord           = (int32_t)parseUint32(field(30));
    r.admin.channel_type         = channel_type_reg_.encode(field(31));
    r.admin.park_facility        = storeStr(32);
    r.admin.park_borough         = park_borough_reg_.encode(field(33));
    r.misc.vehicle_type          = storeStr(34);
    r.misc.taxi_borough          = storeStr(35);
    r.misc.taxi_pickup           = storeStr(36);
    r.misc.bridge_name           = storeStr(37);
    r.misc.bridge_direction      = storeStr(38);
    r.misc.road_ramp             = storeStr(39);
    r.misc.bridge_segment        = storeStr(40);
    r.location.latitude          = parseDouble(field(41));
    r.location.longitude         = parseDouble(field(42));
    r.location.location_point    = storeStr(43);
    return r;
}

void ParallelDataStore::load(const std::string& path, int num_threads) {
    std::ifstream probe(path, std::ios::binary | std::ios::ate);
    if (!probe) { 
        std::cerr << "Cannot open: " << path << "\n"; return;
    }
    int64_t file_size = (int64_t)probe.tellg();
    probe.close();

    std::ifstream hdr(path, std::ios::binary);
    std::string header;
    std::getline(hdr,header);
    int64_t body_start = (int64_t)hdr.tellg();
    hdr.close();

    std::vector<int64_t> boundaries(num_threads+1);
    boundaries[0] = body_start;
    boundaries[num_threads] = file_size;

    for(int i=1; i<num_threads; ++i) {
        int64_t raw = body_start + (int64_t)i * (file_size - body_start) / num_threads;

        std::ifstream snap(path, std::ios::binary);
        snap.seekg(raw);
        char c;
        while (snap.get(c) && c != '\n') {}
        boundaries[i] = (int64_t)snap.tellg();
    }

    std::vector<std::vector<ServiceRequest>> thread_records(num_threads);
    
    #pragma omp parallel num_threads(num_threads)
    {
        int tid = omp_get_thread_num();
        int64_t start = boundaries[tid];
        int64_t end = boundaries[tid+1];

        if (start < end) {
            std::ifstream f(path, std::ios::binary);
            f.seekg(start);

            std::string_view fields[MAX_FIELDS];
            std::string line;
            int64_t pos = start;

            while (pos < end && std::getline(f, line)) {
                pos += (int64_t)line.size() + 1;
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty()) continue;

                int nf = CSVParser::parseLine(line.data(), (int)line.size(), fields, MAX_FIELDS);
                thread_records[tid].push_back(parseRow(fields, nf));
            }
        }
    }

    size_t total = 0;

    for(auto& v : thread_records) total += v.size();
    records_.reserve(total);
    for(auto& v : thread_records) {
        for (auto& r : v) {
            records_.push_back(std::move(r));
        }
    }
}

std::vector<const ServiceRequest*>
ParallelDataStore::searchByZip(uint32_t zip_min, uint32_t zip_max, int num_threads) const {
    std::vector<std::vector<const ServiceRequest*>> local(num_threads);

    #pragma omp parallel num_threads(num_threads)
    {
        int tid = omp_get_thread_num();
        #pragma omp for nowait schedule(static)

        for (int i=0; i< (int)records_.size();++i) 
            if (records_[i].location.zip >= zip_min && records_[i].location.zip <= zip_max) {
                local[tid].push_back(&records_[i]);
            }
    }
    std::vector<const ServiceRequest*> result;
    for (auto& v : local)
        result.insert(result.end(), v.begin(), v.end());
    return result;
}


std::vector<const ServiceRequest*>
ParallelDataStore::searchByDate(uint32_t from, uint32_t to, int num_threads) const {
    std::vector<std::vector<const ServiceRequest*>> local(num_threads);

    #pragma omp parallel num_threads(num_threads)
    {
        int tid = omp_get_thread_num();
        #pragma omp for nowait schedule(static)
        for (int i = 0; i < (int)records_.size(); ++i)
            if (records_[i].time.created >= from && records_[i].time.created <= to)
                local[tid].push_back(&records_[i]);
    }

    std::vector<const ServiceRequest*> result;
    for (auto& v : local) result.insert(result.end(), v.begin(), v.end());
    return result;
}

std::vector<const ServiceRequest*>
ParallelDataStore::searchByBoundingBox(double lat_min, double lat_max,
                                        double lon_min, double lon_max,
                                        int num_threads) const {
    std::vector<std::vector<const ServiceRequest*>> local(num_threads);

    #pragma omp parallel num_threads(num_threads)
    {
        int tid = omp_get_thread_num();
        #pragma omp for nowait schedule(static)
        for (int i = 0; i < (int)records_.size(); ++i) {
            const auto& r = records_[i];
            if (r.location.latitude  >= lat_min && r.location.latitude  <= lat_max &&
                r.location.longitude >= lon_min && r.location.longitude <= lon_max)
                local[tid].push_back(&r);
        }
    }

    std::vector<const ServiceRequest*> result;
    for (auto& v : local) result.insert(result.end(), v.begin(), v.end());
    return result;
}