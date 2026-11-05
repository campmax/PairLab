#include "../src/engine.hpp"
#include <ctime>
#include <functional>
#include <random>
using namespace pl;
int passed = 0, failed = 0;
void check(bool x, const std::string &why) { require(x, why); }
void near(double x, double y, double eps = 1e-8) {
    require(std::abs(x - y) <= eps,
            "Numbers differ: " + std::to_string(x) + " vs " + std::to_string(y));
}
void rejects(const std::function<void()> &fn) {
    bool threw = false;
    try {
        fn();
    } catch (const std::exception &) {
        threw = true;
    }
    check(threw, "Expected rejection");
}
void test(const std::string &name, const std::function<void()> &fn) {
    try {
        fn();
        ++passed;
        std::cout << "PASS " << name << '\n';
    } catch (const std::exception &e) {
        ++failed;
        std::cout << "FAIL " << name << ": " << e.what() << '\n';
    }
}
Dataset fixture() {
    Dataset d;
    std::mt19937 gen(814);
    std::normal_distribution<double> normal;
    double x = 0, e = 0;
    std::time_t t = 1577836800;
    for (int i = 0; i < 1100; ++i) {
        std::tm tm = *std::gmtime(&t);
        while (tm.tm_wday == 0 || tm.tm_wday == 6) {
            t += 86400;
            tm = *std::gmtime(&t);
        }
        char buf[16];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
        std::string date = buf;
        t += 86400;
        d.dates.push_back(date);
        x += .012 * normal(gen);
        e = .75 * e + .022 * normal(gen);
        for (const auto &symbol : {"A", "B", "SPY"}) {
            auto &a = d.assets[symbol];
            a.symbol = symbol;
            double p = 100 * std::exp(symbol == std::string("A") ? 1.2 * x + e : x);
            double open = a.bars.empty() ? p : a.bars.back().close * (1 + .001 * normal(gen));
            a.bars.push_back({date, dayNumber(date), open, std::max(open, p) * 1.01,
                              std::min(open, p) * .99, p, 0, 1});
            a.logTotal.push_back(std::log(p / 100));
        }
    }
    return d;
}
Config fixtureConfig(const Dataset &d) {
    Config c;
    c.pairs = {{"A", "B"}};
    c.start = d.dates[504];
    c.end = "2030-01-01";
    c.holdout = c.end;
    c.maxBetaDrift = 2;
    return c;
}
int main() {
    test("Dates validate leap days and reject malformed dates", [] {
        near(dayNumber("2024-03-01") - dayNumber("2024-02-28"), 2);
        rejects([] { dayNumber("2023-02-29"); });
        rejects([] { dayNumber("2024-02-1x"); });
    });
    test("Configuration rejects unknown keys",
         [] { rejects([] { parseConfig({{"entyr", 2}}); }); });
    test("Configuration rejects fractional windows and overlapping pairs", [] {
        rejects([] { parseConfig({{"training", 504.5}}); });
        rejects([] { parseConfig({{"pairs", {{"A", "B"}, {"A", "C"}}}}); });
    });
    test("Configuration rejects contradictory thresholds", [] {
        rejects([] { parseConfig({{"exit", 2.5}}); });
        rejects([] { parseConfig({{"borrow_rate", -1}}); });
    });
    test("OLS recovers intercept and slope", [] {
        auto r = line({1, 2, 3, 4, 5}, {5, 8, 11, 14, 17});
        near(r.coef[0], 2);
        near(r.coef[1], 3);
        near(r.r2, 1);
    });
    test("Singular regression rejected", [] { rejects([] { line({1, 1, 1}, {2, 3, 4}); }); });
    test("Cointegration uses two-series finite-sample critical value", [] {
        near(egCritical(504), -3.918345368300095, 1e-6);
        check(egCritical(504) < -3.8, "Wrong distribution");
    });
    test("Adverse execution cost applies to buys and sells", [] {
        Config c;
        Ledger b;
        b.cash = 1000;
        b.fill(true, 2, 100, c);
        near(b.cash, 798.9);
        b.fill(true, -2, 100, c);
        near(b.cash, 997.8);
        near(b.slippage, .2);
        near(b.fees, 2);
    });
    test("Short proceeds are not profit", [] {
        Config c;
        c.slipBps = 0;
        c.commission = 0;
        Ledger b;
        b.cash = 1000;
        b.fill(true, -5, 100, c);
        near(b.cash, 1500);
        near(b.equity(100, 0), 1000);
        near(b.equity(110, 0), 950);
    });
    test("Long dividends credited and short dividends owed", [] {
        Ledger b;
        b.cash = 1000;
        b.qa = 10;
        b.qb = -20;
        Bar a, bbar;
        a.dividend = 2;
        bbar.dividend = 3;
        b.actions(a, bbar);
        near(b.cash, 960);
        near(b.dividends, -40);
    });
    test("Split conserves long and short marked value", [] {
        Ledger b;
        b.cash = 1000;
        b.qa = 10;
        b.qb = -20;
        double old = b.equity(100, 50);
        Bar a, bb;
        a.split = 2;
        bb.split = 4;
        b.actions(a, bb);
        near(b.equity(50, 12.5), old);
        near(b.qa, 20);
        near(b.qb, -80);
    });
    test("Split-date dividends apply to new share count", [] {
        Ledger b;
        b.qa = 10;
        Bar a, bb;
        a.split = 2;
        a.dividend = 1;
        b.actions(a, bb);
        near(b.cash, 20);
    });
    test("Borrow uses calendar days including weekends", [] {
        Config c;
        c.borrow = .365;
        Ledger b;
        b.cash = 2000;
        b.qa = -10;
        b.carry(100, 0, 3, c);
        near(b.borrow, 3);
        near(b.cash, 1997);
    });
    test("Restricted short collateral is excluded from free cash", [] {
        Config c;
        c.borrow = 0;
        c.marginRate = .365;
        Ledger b;
        b.cash = 900;
        b.qa = -10;
        b.carry(100, 0, 1, c);
        near(b.interest, .1);
    });
    test("Cash with no position has no invented earnings", [] {
        Config c;
        Ledger b;
        b.cash = 1000;
        b.carry(100, 100, 50, c);
        near(b.cash, 1000);
    });
    test("Drawdown includes initial equity", [] {
        auto m = summaryMetrics({100, 90, 95}, {"2020-01-01", "2020-01-02", "2020-01-03"});
        near(m["max_drawdown"], .1);
        near(m["return"], -.05);
    });
    test("Zero-volatility Sharpe is undefined", [] {
        auto m = summaryMetrics({100, 100, 100}, {"2020-01-01", "2020-01-02", "2020-01-03"});
        check(m["sharpe"].is_null(), "Sharpe should be null");
    });
    auto d = fixture();
    auto c = fixtureConfig(d);
    J result = runBacktest(d, c, "Test");
    test("Fixture produces meaningful completed trades",
         [&] { check(result["trades"].size() > 10, "No meaningful trading coverage"); });
    test("All entries follow signals; no same-close execution", [&] {
        for (const auto &t : result["trades"]) {
            check(t["entry_date"].get<std::string>() > t["signal_date"].get<std::string>(),
                  "Lookahead entry");
            check(t["exit_date"].get<std::string>() > t["exit_signal_date"].get<std::string>(),
                  "Lookahead exit");
        }
    });
    test("Training ends before each trading window", [&] {
        for (const auto &f : result["pairs"][0]["folds"])
            check(f["train_end"].get<std::string>() < f["trade_start"].get<std::string>(),
                  "Training leak");
    });
    test("Closed trade P&L reconciles to ending equity", [&] {
        double pnl = 0;
        for (const auto &t : result["trades"])
            pnl += t["net_pnl"].get<double>();
        near(c.capital + pnl, result["metrics"]["end_equity"], 1e-7);
    });
    test("Final scheduled open leaves no positions", [&] {
        auto r = result["pairs"][0]["daily"].back();
        near(r["qty_a"], 0);
        near(r["qty_b"], 0);
    });
    test("Future data changes cannot change earlier trades or models", [&] {
        auto changed = d;
        for (size_t i = 900; i < d.dates.size(); ++i) {
            auto &a = changed.assets["A"];
            a.logTotal[i] += .25;
            a.bars[i].open *= 1.28;
            a.bars[i].close *= 1.28;
        }
        auto other = runBacktest(changed, c, "Changed future");
        for (size_t i = 0; i < 396; ++i)
            check(result["daily"][i] == other["daily"][i], "Future changed historical equity");
        for (const auto &f : other["pairs"][0]["folds"])
            if (f["trade_start"].get<std::string>() < d.dates[900]) {
                bool found = false;
                for (const auto &old : result["pairs"][0]["folds"])
                    if (old["trade_start"] == f["trade_start"]) {
                        check(old == f, "Future changed model");
                        found = true;
                    }
                check(found, "Missing fold");
            }
    });
    test("Extra execution delay is respected", [&] {
        auto delayed = c;
        delayed.delay = 2;
        auto r = runBacktest(d, delayed, "Delayed");
        for (const auto &t : r["trades"]) {
            auto a = std::lower_bound(d.dates.begin(), d.dates.end(),
                                      t["signal_date"].get<std::string>()),
                 b = std::lower_bound(d.dates.begin(), d.dates.end(),
                                      t["entry_date"].get<std::string>());
            check(b - a == 2, "Incorrect entry delay");
        }
    });
    test("Failed screen keeps sleeve in cash", [&] {
        auto blocked = c;
        blocked.minSigma = 10;
        auto r = runBacktest(d, blocked, "Blocked");
        check(r["trades"].empty(), "Screen bypassed");
        near(r["metrics"]["end_equity"], c.capital);
    });
    test("No-cost run has zero reported trading/holding costs", [&] {
        auto x = c;
        x.slipBps = x.commission = x.borrow = x.marginRate = 0;
        auto r = runBacktest(d, x, "Free");
        for (auto k : {"fees", "slippage", "borrow", "interest"})
            near(r["metrics"][k], 0);
    });
    test("Explicit split leaves all portfolio values invariant", [&] {
        auto splitData = d;
        size_t splitAt = 650;
        auto &asset = splitData.assets["A"];
        for (size_t i = splitAt; i < asset.bars.size(); ++i) {
            asset.bars[i].open /= 2;
            asset.bars[i].high /= 2;
            asset.bars[i].low /= 2;
            asset.bars[i].close /= 2;
        }
        asset.bars[splitAt].split = 2;
        auto splitRun = runBacktest(splitData, c, "Split equivalent");
        for (size_t i = 0; i < result["daily"].size(); ++i) {
            near(splitRun["daily"][i]["equity"], result["daily"][i]["equity"], 1e-7);
            near(splitRun["daily"][i]["basket"], result["daily"][i]["basket"], 1e-7);
        }
    });
    test("Risk stop executes at a later open and blocks its window", [&] {
        auto shocked = d;
        const auto &firstTrade = result["trades"][0];
        auto entry = std::lower_bound(d.dates.begin(), d.dates.end(),
                                      firstTrade["entry_date"].get<std::string>());
        size_t index = size_t(entry - d.dates.begin());
        int direction = firstTrade["direction"].get<int>();
        double factor = direction == 1 ? .70 : 1.30;
                                                                                
        shocked.assets["A"].bars[index].close *= factor;
        shocked.assets["A"].logTotal[index] += std::log(factor);
        auto r = runBacktest(shocked, c, "Shock");
        const auto &t = r["trades"][0];
        check(t["reason"] == "capital_stop" || t["reason"] == "spread_stop", "Missing risk exit");
        check(t["exit_date"] == d.dates[index + 1], "Risk exit did not wait for next open");
        size_t boundary = 504 + ((index - 504) / size_t(c.window) + 1) * size_t(c.window);
        for (size_t k = 1; k < r["trades"].size(); ++k)
            check(r["trades"][k]["entry_date"].get<std::string>() >= d.dates[boundary],
                  "Risk window reopened");
    });
    test("Deterministic rerun is identical",
         [&] { check(runBacktest(d, c, "Test") == result, "Nondeterministic results"); });
    std::cout << passed << " passed; " << failed << " failed\n";
    return failed ? 1 : 0;
}
