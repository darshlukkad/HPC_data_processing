#include "DataStore.hpp"
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <ctime>

static const int MAX_FIELDS = 50;
static const uint32_t RES_POOL_BYTES = 3200u * 1024u * 1024u;
static const uint32_t POOL_BYTES = 3500u * 1024u * 1024u;  // 3.5 GB, fits in uint32_t

DataStore::DataStore(): pool_(POOL_BYTES), res_pool_(RES_POOL_BYTES) {}


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
    return std::strtoull(buf, nullptr, 10);
}

static double parseDouble(std::string_view sv) {
    if (sv.empty()) return 0.0;
    char buf[32];
    size_t n = std::min(sv.size(), sizeof(buf)-1);
    std::memcpy(buf, sv.data(), n);
    buf[n] = '\0';
    return std::strtod(buf, nullptr);
}

static uint32_t parseDateField(std::string_view sv) {
    if (sv.empty()) return 0;
    return parseDateTime(sv.data(), (int)sv.size());
}

void DataStore::load(const std::string& path) {
    {
        std::ifstream f(path, std::ios::binary);
        if (!f) { std::cerr << "Cannot open: " << path << "\n"; return; }
        char buf[65536];
        size_t count = 0;
        while (f.read(buf, sizeof(buf)) || f.gcount() > 0) {
            auto n = f.gcount();
            for (int i = 0; i < n; i++)
                if (buf[i] == '\n') count++;
        }
        records_.reserve(count);
    }

    std::ifstream f(path, std::ios::binary);
    if (!f) return;

    std::string line;
    std::getline(f, line);

    std::string_view fields[MAX_FIELDS];
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty()) continue;

        int nfields = CSVParser::parseLine(line.data(), (int)line.size(),
                                           fields, MAX_FIELDS);
        records_.push_back(parseRow(fields, nfields));
    }
}

ServiceRequest DataStore::parseRow(std::string_view* f, int n) {
    auto field = [&](int i) -> std::string_view {
        return (i < n) ? f[i] : std::string_view{};
    };

    auto storeStr = [&](int i) -> StringRef {
        auto sv = field(i);
        if (sv.empty()) return {0, 0};
        return pool_.store(sv.data(), (uint16_t)sv.size());
    };

    ServiceRequest r{};

    // Verified column order (0-indexed) from actual CSV header:
    // 0=Unique Key, 1=Created Date, 2=Closed Date, 3=Agency, 4=Agency Name,
    // 5=Problem, 6=Problem Detail, 7=Additional Details, 8=Location Type,
    // 9=Incident Zip, 10=Incident Address, 11=Street Name, 12=Cross Street 1,
    // 13=Cross Street 2, 14=Intersection Street 1, 15=Intersection Street 2,
    // 16=Address Type, 17=City, 18=Landmark, 19=Facility Type, 20=Status,
    // 21=Due Date, 22=Resolution Description, 23=Resolution Action Updated Date,
    // 24=Community Board, 25=Council District, 26=Police Precinct, 27=BBL,
    // 28=Borough, 29=X Coordinate, 30=Y Coordinate, 31=Open Data Channel Type,
    // 32=Park Facility Name, 33=Park Borough, 34=Vehicle Type,
    // 35=Taxi Company Borough, 36=Taxi Pick Up Location, 37=Bridge Highway Name,
    // 38=Bridge Highway Direction, 39=Road Ramp, 40=Bridge Highway Segment,
    // 41=Latitude, 42=Longitude, 43=Location

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

void DataStore::printRecord(const ServiceRequest* r, std::ostream& os) const {
    auto gs = [&](StringRef ref) -> std::string_view {
        if (ref.length == 0) return "";
        return {pool_.get(ref), ref.length};
    };
    auto gr = [&](StringRef ref) -> std::string_view {
        if (ref.length == 0) return "";
        return {res_pool_.get(ref), ref.length};
    };
    auto fmtTime = [](uint32_t epoch) -> std::string {
        if (epoch == 0) return "N/A";
        time_t t = (time_t)epoch;
        char buf[32];
        struct tm tm{};
        gmtime_r(&t, &tm);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        return buf;
    };

    os << "  unique_key:         " << r->unique_key                                         << "\n"
       << "  created:            " << fmtTime(r->time.created)                              << "\n"
       << "  closed:             " << fmtTime(r->time.closed)                               << "\n"
       << "  due:                " << fmtTime(r->time.due)                                  << "\n"
       << "  resolution_date:    " << fmtTime(r->time.resolution)                           << "\n"
       << "  agency:             " << agency_reg_.decode(r->service.agency)                 << "\n"
       << "  agency_name:        " << gs(r->service.agency_name)                            << "\n"
       << "  problem:            " << problem_reg_.decode(r->service.problem)               << "\n"
       << "  problem_detail:     " << problem_detail_reg_.decode(r->service.problem_detail) << "\n"
       << "  additional_details: " << gs(r->service.additional_details)                     << "\n"
       << "  location_type:      " << location_type_reg_.decode(r->service.location_type)   << "\n"
       << "  status:             " << status_reg_.decode(r->service.status)                 << "\n"
       << "  facility_type:      " << gs(r->service.facility_type)                          << "\n"
       << "  resolution_desc:    " << gr(r->service.resolution_desc)                        << "\n"
       << "  zip:                " << r->location.zip                                       << "\n"
       << "  address:            " << gs(r->location.address)                               << "\n"
       << "  street_name:        " << gs(r->location.street_name)                           << "\n"
       << "  cross_street_1:     " << gs(r->location.cross_street_1)                        << "\n"
       << "  cross_street_2:     " << gs(r->location.cross_street_2)                        << "\n"
       << "  intersection_1:     " << gs(r->location.intersection_1)                        << "\n"
       << "  intersection_2:     " << gs(r->location.intersection_2)                        << "\n"
       << "  address_type:       " << address_type_reg_.decode(r->location.address_type)    << "\n"
       << "  city:               " << gs(r->location.city)                                  << "\n"
       << "  landmark:           " << gs(r->location.landmark)                              << "\n"
       << "  borough:            " << borough_reg_.decode(r->location.borough)              << "\n"
       << "  latitude:           " << r->location.latitude                                  << "\n"
       << "  longitude:          " << r->location.longitude                                 << "\n"
       << "  x_coord:            " << r->location.x_coord                                   << "\n"
       << "  y_coord:            " << r->location.y_coord                                   << "\n"
       << "  location_point:     " << gs(r->location.location_point)                        << "\n"
       << "  community_board:    " << r->admin.community_board                              << "\n"
       << "  council_district:   " << r->admin.council_district                             << "\n"
       << "  police_precinct:    " << r->admin.police_precinct                              << "\n"
       << "  bbl:                " << r->admin.bbl                                          << "\n"
       << "  channel_type:       " << channel_type_reg_.decode(r->admin.channel_type)       << "\n"
       << "  park_facility:      " << gs(r->admin.park_facility)                            << "\n"
       << "  park_borough:       " << park_borough_reg_.decode(r->admin.park_borough)       << "\n"
       << "  vehicle_type:       " << gs(r->misc.vehicle_type)                              << "\n"
       << "  taxi_borough:       " << gs(r->misc.taxi_borough)                              << "\n"
       << "  taxi_pickup:        " << gs(r->misc.taxi_pickup)                               << "\n"
       << "  bridge_name:        " << gs(r->misc.bridge_name)                               << "\n"
       << "  bridge_direction:   " << gs(r->misc.bridge_direction)                          << "\n"
       << "  road_ramp:          " << gs(r->misc.road_ramp)                                 << "\n"
       << "  bridge_segment:     " << gs(r->misc.bridge_segment)                            << "\n";
}

std::vector<const ServiceRequest*>
DataStore::searchByZip(uint32_t zip_min, uint32_t zip_max) const {
    std::vector<const ServiceRequest*> result;
    for (const auto& r : records_)
        if (r.location.zip >= zip_min && r.location.zip <= zip_max)
            result.push_back(&r);
    return result;
}

std::vector<const ServiceRequest*>
DataStore::searchByDate(uint32_t from, uint32_t to) const {
    std::vector<const ServiceRequest*> result;
    for (const auto& r : records_)
        if (r.time.created >= from && r.time.created <= to)
            result.push_back(&r);
    return result;
}

std::vector<const ServiceRequest*>
DataStore::searchByBoundingBox(double lat_min, double lat_max,
                                double lon_min, double lon_max) const {
    std::vector<const ServiceRequest*> result;
    for (const auto& r : records_)
        if (r.location.latitude  >= lat_min && r.location.latitude  <= lat_max &&
            r.location.longitude >= lon_min && r.location.longitude <= lon_max)
            result.push_back(&r);
    return result;
}