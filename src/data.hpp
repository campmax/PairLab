#pragma once
#include "common.hpp"
namespace pl {
struct Bar {
    std::string date;
    int day = 0;
    double open = 0, high = 0, low = 0, close = 0, dividend = 0, split = 1;
};
struct Asset {
    std::string symbol;
    std::vector<Bar> bars;
    std::vector<double> logTotal;
    std::string hash;
};
inline Asset loadAsset(const fs::path &file, const std::string &symbol) {
    Asset a;
    a.symbol = symbol;
    auto text = readText(file);
    a.hash = fingerprint(text);
    std::istringstream in(text);
    std::string line;
    std::getline(in, line);
    require(fields(line) == std::vector<std::string>(
                                {"date", "open", "high", "low", "close", "dividend", "split"}),
            "Wrong CSV schema: " + symbol);
    while (std::getline(in, line)) {
        require(!line.empty(), "Blank CSV row");
        auto v = fields(line);
        require(v.size() == 7, "Wrong field count: " + symbol);
        Bar b;
        b.date = v[0];
        b.day = dayNumber(b.date);
        b.open = number(v[1]);
        b.high = number(v[2]);
        b.low = number(v[3]);
        b.close = number(v[4]);
        b.dividend = number(v[5]);
        b.split = number(v[6]);
        require(std::min({b.open, b.high, b.low, b.close}) > 0 &&
                    b.low <= std::min(b.open, b.close) + 1e-7 &&
                    b.high + 1e-7 >= std::max(b.open, b.close) && b.dividend >= 0 && b.split > 0,
                "Invalid OHLC/actions: " + symbol + " " + b.date);
        require(a.bars.empty() || b.day > a.bars.back().day, "Unordered/duplicate date: " + symbol);
        double logValue = 0;
        if (!a.bars.empty()) {
            double ratio = b.split * (b.close + b.dividend) / a.bars.back().close;
            require(ratio > .05 && ratio < 20, "Extreme action-adjusted return; verify input");
            logValue = a.logTotal.back() + std::log(ratio);
        }
        a.bars.push_back(b);
        a.logTotal.push_back(logValue);
    }
    require(a.bars.size() >= 127, "Insufficient rows: " + symbol);
    return a;
}
struct Dataset {
    std::map<std::string, Asset> assets;
    J provenance;
    std::vector<std::string> dates;
};
inline Dataset loadData(const fs::path &dir, const Config &c) {
    Dataset d;
    d.provenance = readJson(dir / "provenance.json");
    require(d.provenance.value("schema", "") == "pairlab.data.v1", "Unsupported data metadata");
    require(d.provenance.value("price_basis", "") == "reconstructed_as_traded",
            "Only reconstructed-as-traded data supported");
    std::set<std::string> symbols = {c.benchmark};
    for (const auto &p : c.pairs) {
        symbols.insert(p.first);
        symbols.insert(p.second);
    }
    for (const auto &symbol : symbols) {
        auto a = loadAsset(dir / (symbol + ".csv"), symbol);
        if (d.dates.empty()) {
            for (const auto &b : a.bars)
                d.dates.push_back(b.date);
        } else {
            require(a.bars.size() == d.dates.size(),
                    "Calendars differ; no silent dropping/forward filling");
            for (size_t i = 0; i < d.dates.size(); ++i)
                require(a.bars[i].date == d.dates[i], "Calendar mismatch: " + symbol);
        }
        require(d.provenance.at("files").at(symbol).at("fnv1a64") == a.hash,
                "CSV fingerprint does not match provenance: " + symbol);
        d.assets.emplace(symbol, std::move(a));
    }
    require(d.dates.size() > size_t(c.training + c.window),
            "Insufficient training and evaluation data");
    return d;
}
}                
