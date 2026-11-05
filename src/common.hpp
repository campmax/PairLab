#pragma once
#include "../third_party/json.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace pl {
using J = nlohmann::json;
namespace fs = std::filesystem;
inline void require(bool x, const std::string &message) {
    if (!x)
        throw std::runtime_error(message);
}
inline std::string readText(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    require(bool(f), "Cannot read " + p.string());
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}
inline J readJson(const fs::path &p) { return J::parse(readText(p)); }
inline void writeText(const fs::path &p, const std::string &s) {
    std::ofstream f(p, std::ios::binary);
    require(bool(f), "Cannot write " + p.string());
    f << s;
    require(bool(f), "Write failed " + p.string());
}
inline double number(const std::string &s) {
    size_t used = 0;
    double x = std::stod(s, &used);
    require(used == s.size() && std::isfinite(x), "Invalid numeric field: " + s);
    return x;
}
inline std::vector<std::string> fields(std::string s) {
    if (!s.empty() && s.back() == '\r')
        s.pop_back();
    std::vector<std::string> v;
    std::stringstream f(s);
    std::string x;
    while (std::getline(f, x, ','))
        v.push_back(x);
    return v;
}
                                                                                 
inline int dayNumber(const std::string &s) {
    require(s.size() == 10 && s[4] == '-' && s[7] == '-', "Invalid ISO date: " + s);
    for (size_t i = 0; i < s.size(); ++i)
        if (i != 4 && i != 7)
            require(s[i] >= '0' && s[i] <= '9', "Invalid ISO date");
    int y = std::stoi(s.substr(0, 4)), m = std::stoi(s.substr(5, 2)), d = std::stoi(s.substr(8, 2));
    const int lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
    require(y >= 1900 && y <= 2200 && m >= 1 && m <= 12 && d >= 1 &&
                d <= lengths[m - 1] + (m == 2 && leap),
            "Invalid calendar date");
    y -= m <= 2;
    int era = y / 400;
    unsigned yo = unsigned(y - era * 400), mp = unsigned(m + (m > 2 ? -3 : 9));
    return era * 146097 + int(yo * 365 + yo / 4 - yo / 100 + (153 * mp + 2) / 5 + unsigned(d) - 1);
}
inline std::string fingerprint(const std::string &s) {
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << h;
    return out.str();
}
inline double mean(const std::vector<double> &x) {
    require(!x.empty(), "Empty sample");
    return std::accumulate(x.begin(), x.end(), 0.0) / double(x.size());
}
inline double variance(const std::vector<double> &x) {
    if (x.size() < 2)
        return 0;
    double m = mean(x), v = 0;
    for (double a : x)
        v += (a - m) * (a - m);
    return v / double(x.size() - 1);
}
inline double median(std::vector<double> x) {
    require(!x.empty(), "Empty median");
    std::sort(x.begin(), x.end());
    size_t n = x.size();
    return n % 2 ? x[n / 2] : (x[n / 2 - 1] + x[n / 2]) / 2;
}
inline J finite(double x) { return std::isfinite(x) ? J(x) : J(nullptr); }
struct Config {
    int training = 504, window = 63, adfLag = 1, maxHold = 20, delay = 1;
    double capital = 100000, entry = 2, exit = .5, stop = 4;
    double slipBps = 5, commission = 1, borrow = .03, marginRate = .08, gross = 1,
           maintenance = .30, pairStop = .10;
    double minBeta = .2, maxBeta = 5, minHalf = 1, maxHalf = 60, maxBetaDrift = .5, minSigma = .005;
    std::string start = "2012-01-01", end = "2024-01-01", holdout = "2024-01-01", benchmark = "SPY";
    std::vector<std::pair<std::string, std::string>> pairs = {
        {"KO", "PEP"}, {"XOM", "CVX"}, {"V", "MA"}, {"JPM", "BAC"}};
};
inline Config parseConfig(const J &j) {
    Config c;
    require(j.is_object(), "Config must be an object");
    const std::set<std::string> keys = {
        "training",       "window",      "adf_lag",     "max_hold",      "delay",
        "capital",        "entry",       "exit",        "stop",          "slippage_bps",
        "commission",     "borrow_rate", "margin_rate", "gross",         "maintenance",
        "pair_stop",      "min_beta",    "max_beta",    "min_half_life", "max_half_life",
        "max_beta_drift", "min_sigma",   "start",       "end_exclusive", "holdout_start",
        "benchmark",      "pairs"};
    for (auto it = j.begin(); it != j.end(); ++it)
        require(keys.count(it.key()) != 0, "Unknown config key: " + it.key());
    auto integer = [&](const char *k, int v, int lo, int hi) {
        double n = j.value(k, double(v));
        require(std::isfinite(n) && n == std::floor(n) && n >= lo && n <= hi,
                std::string("Invalid ") + k);
        return int(n);
    };
    c.training = integer("training", 504, 126, 2520);
    c.window = integer("window", 63, 5, 252);
    c.adfLag = integer("adf_lag", 1, 0, 10);
    c.maxHold = integer("max_hold", 20, 1, 252);
    c.delay = integer("delay", 1, 1, 5);
    auto real = [&](const char *k, double v, double lo, double hi) {
        double n = j.value(k, v);
        require(std::isfinite(n) && n >= lo && n <= hi, std::string("Invalid ") + k);
        return n;
    };
    c.capital = real("capital", 1e5, 1000, 1e10);
    c.entry = real("entry", 2, .5, 6);
    c.exit = real("exit", .5, 0, 3);
    c.stop = real("stop", 4, 1, 12);
    require(c.exit < c.entry && c.entry < c.stop, "Require exit < entry < stop");
    c.slipBps = real("slippage_bps", 5, 0, 500);
    c.commission = real("commission", 1, 0, 100);
    c.borrow = real("borrow_rate", .03, 0, 2);
    c.marginRate = real("margin_rate", .08, 0, 2);
    c.gross = real("gross", 1, .1, 2);
    c.maintenance = real("maintenance", .3, .1, .9);
    require(c.gross * c.maintenance < 1, "Initial gross exceeds maintenance limit");
    c.pairStop = real("pair_stop", .1, .01, .9);
    c.minBeta = real("min_beta", .2, .01, 10);
    c.maxBeta = real("max_beta", 5, .1, 20);
    require(c.minBeta < c.maxBeta, "Invalid beta range");
    c.minHalf = real("min_half_life", 1, .1, 250);
    c.maxHalf = real("max_half_life", 60, 1, 500);
    require(c.minHalf < c.maxHalf, "Invalid half-life range");
    c.maxBetaDrift = real("max_beta_drift", .5, .01, 5);
    c.minSigma = real("min_sigma", .005, 1e-6, 1);
    c.start = j.value("start", c.start);
    c.end = j.value("end_exclusive", c.end);
    c.holdout = j.value("holdout_start", c.holdout);
    dayNumber(c.start);
    dayNumber(c.end);
    dayNumber(c.holdout);
    require(c.start < c.end, "Empty evaluation dates");
    c.benchmark = j.value("benchmark", c.benchmark);
    if (j.contains("pairs")) {
        c.pairs.clear();
        for (const auto &p : j.at("pairs")) {
            require(p.is_array() && p.size() == 2, "Pair needs two symbols");
            c.pairs.emplace_back(p[0].get<std::string>(), p[1].get<std::string>());
        }
    }
    require(!c.pairs.empty() && c.pairs.size() <= 4,
            "Use 1–4 predefined pairs; per-window family-wise screen assumes at most four tests");
    std::set<std::string> symbols;
    for (const auto &p : c.pairs) {
        for (const auto &x : {p.first, p.second}) {
            require(!x.empty() && x.size() <= 12 &&
                        x.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-") ==
                            std::string::npos,
                    "Invalid symbol");
            require(symbols.insert(x).second,
                    "Pairs must be disjoint to preserve separate capital sleeves");
        }
    }
    require(!c.benchmark.empty() &&
                c.benchmark.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-") ==
                    std::string::npos,
            "Invalid benchmark");
    return c;
}
}                
