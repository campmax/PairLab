#pragma once
#include "statistics.hpp"
namespace pl {
struct Ledger {
    double cash = 0, qa = 0, qb = 0, fees = 0, slippage = 0, borrow = 0, interest = 0,
           dividends = 0, turnover = 0;
    double equity(double a, double b) const { return cash + qa * a + qb * b; }
    double shortValue(double a, double b) const {
        return std::max(0.0, -qa * a) + std::max(0.0, -qb * b);
    }
    double gross(double a, double b) const { return std::abs(qa * a) + std::abs(qb * b); }
    void fill(bool first, double delta, double price, const Config &c) {
        if (std::abs(delta) < 1e-12)
            return;
        double slip = std::abs(delta) * price * c.slipBps / 10000;
        cash -= delta * price + slip + c.commission;
        fees += c.commission;
        slippage += slip;
        turnover += std::abs(delta) * price;
        (first ? qa : qb) += delta;
    }
    void actions(const Bar &a, const Bar &b) {
        qa *= a.split;
        qb *= b.split;
        double cashflow = qa * a.dividend + qb * b.dividend;
        cash += cashflow;
        dividends += cashflow;
    }
    void carry(double a, double b, int days, const Config &c) {
        double s = shortValue(a, b), bc = s * c.borrow * days / 365.0,
               mc = std::max(0.0, s - cash) * c.marginRate * days / 365.0;
        cash -= bc + mc;
        borrow += bc;
        interest += mc;
    }
};
struct Order {
    int due = -1, direction = 0;
    double qa = 0, qb = 0, z = 0;
    std::string signal, reason;
};
struct Sleeve {
    std::string a, b, id;
    Ledger book;
    Model model;
    Order pending;
    int direction = 0, entryIndex = 0;
    bool blocked = false;
    double entryEquity = 0, entryZ = 0;
    std::string entryDate, signalDate;
    J trades = J::array(), daily = J::array(), folds = J::array(), events = J::array();
};
inline J summaryMetrics(const std::vector<double> &equity, const std::vector<std::string> &dates) {
    require(equity.size() == dates.size() && equity.size() > 1, "Invalid equity series");
    double peak = equity[0], dd = 0;
    std::vector<double> rets;
    for (size_t i = 1; i < equity.size(); ++i) {
        require(equity[i - 1] > 0 && equity[i] > 0, "Bankrupt portfolio: simulation stopped");
        rets.push_back(equity[i] / equity[i - 1] - 1);
        peak = std::max(peak, equity[i]);
        dd = std::max(dd, 1 - equity[i] / peak);
    }
    double vol = std::sqrt(variance(rets)),
           days = dayNumber(dates.back()) - dayNumber(dates.front());
    return {{"return", equity.back() / equity.front() - 1},
            {"cagr", std::pow(equity.back() / equity.front(), 365.25 / days) - 1},
            {"max_drawdown", dd},
            {"sharpe", vol > 1e-12 ? J(mean(rets) / vol * std::sqrt(252.0)) : J(nullptr)},
            {"volatility", vol * std::sqrt(252.0)},
            {"end_equity", equity.back()},
            {"start_equity", equity.front()},
            {"sessions", rets.size()}};
}
inline void closePosition(Sleeve &s, const Bar &a, const Bar &b, const Config &c,
                          const std::string &reason, const std::string &signal, int index) {
    if (!s.direction)
        return;
    double before = s.book.equity(a.open, b.open);
    double qa = s.book.qa, qb = s.book.qb;
    s.book.fill(true, -qa, a.open, c);
    s.book.fill(false, -qb, b.open, c);
    s.events.push_back({{"date", a.date},
                        {"type", "exit"},
                        {"signal_date", signal},
                        {"reason", reason},
                        {"delta_a", -qa},
                        {"delta_b", -qb},
                        {"open_a", a.open},
                        {"open_b", b.open},
                        {"cash", s.book.cash},
                        {"equity_before", before}});
    s.trades.push_back({{"pair", s.id},
                        {"entry_date", s.entryDate},
                        {"signal_date", s.signalDate},
                        {"exit_date", a.date},
                        {"exit_signal_date", signal},
                        {"direction", s.direction},
                        {"entry_z", s.entryZ},
                        {"held_sessions", index - s.entryIndex},
                        {"reason", reason},
                        {"net_pnl", s.book.cash - s.entryEquity},
                        {"sleeve_return", s.book.cash / s.entryEquity - 1}});
    s.direction = 0;
    s.pending = Order{};
}
inline J runBacktest(const Dataset &data, const Config &c, const std::string &name) {
    auto begin = std::lower_bound(data.dates.begin(), data.dates.end(), c.start);
    auto end = std::lower_bound(data.dates.begin(), data.dates.end(), c.end);
    size_t first = std::max(size_t(c.training), size_t(begin - data.dates.begin())),
           last = size_t(end - data.dates.begin());
    require(last > first + size_t(c.window), "Not enough evaluation sessions");
    --last;
    const double initial = c.capital / double(c.pairs.size());
    std::vector<Sleeve> sleeves;
    for (const auto &p : c.pairs) {
        Sleeve s;
        s.a = p.first;
        s.b = p.second;
        s.id = s.a + " / " + s.b;
        s.book.cash = initial;
        sleeves.push_back(s);
    }
    std::vector<std::string> dates = {data.dates[first - 1]};
    std::vector<double> equity = {c.capital}, spyEquity = {c.capital}, basketEquity = {c.capital};
    J daily = J::array();
    Ledger benchmark;
    benchmark.cash = c.capital;
    std::map<std::string, Ledger> basket;
    std::set<std::string> all;
    for (auto &s : sleeves) {
        all.insert(s.a);
        all.insert(s.b);
    }
    for (auto &symbol : all)
        basket[symbol].cash = c.capital / double(all.size());
    int activeDays = 0;
    double peak = c.capital, spyPeak = c.capital, basketPeak = c.capital;
    bool allFinite = true;
    for (size_t i = first; i <= last; ++i) {
        bool boundary = (i - first) % size_t(c.window) == 0;
        int days =
            data.assets.at(c.benchmark).bars[i].day - data.assets.at(c.benchmark).bars[i - 1].day;
        double eq = 0, gross = 0, net = 0;
        int active = 0;
        for (auto &s : sleeves) {
            const auto &assetA = data.assets.at(s.a);
            const auto &assetB = data.assets.at(s.b);
            const auto &a = assetA.bars[i];
            const auto &b = assetB.bars[i];
            const auto &pa = assetA.bars[i - 1];
            const auto &pb = assetB.bars[i - 1];
            if (i > first)
                s.book.carry(pa.close, pb.close, days, c);
            s.book.actions(a, b);
            if (s.pending.direction) {
                s.pending.qa *= a.split;
                s.pending.qb *= b.split;
            }
            if (boundary || i == last) {
                closePosition(s, a, b, c, i == last ? "scheduled_end" : "window_end",
                              data.dates[i - 1], int(i));
                s.pending = Order{};
            }
            if (boundary) {
                s.model = fitModel(assetA, assetB, i - size_t(c.training), i, c);
                s.blocked = false;
                J f = modelJson(s.model);
                f["train_start"] = data.dates[i - size_t(c.training)];
                f["train_end"] = data.dates[i - 1];
                f["trade_start"] = a.date;
                f["trade_end"] = data.dates[std::min(last, i + size_t(c.window) - 1)];
                s.folds.push_back(f);
            }
            if (s.pending.due == int(i) && i != last) {
                Order order = s.pending;
                s.pending = Order{};
                if (order.direction == 0)
                    closePosition(s, a, b, c, order.reason, order.signal, int(i));
                else if (!s.direction && !s.blocked) {
                    double capital = s.book.equity(a.open, b.open),
                           proposed = std::abs(order.qa * a.open) + std::abs(order.qb * b.open),
                           cost = proposed * c.slipBps / 10000 + 2 * c.commission;
                    if (capital - cost <= c.maintenance * proposed ||
                        proposed > 1.1 * c.gross * capital) {
                        s.events.push_back({{"date", a.date},
                                            {"type", "rejected_entry"},
                                            {"signal_date", order.signal},
                                            {"reason", "Opening gap/capital limit"}});
                    } else {
                        s.entryEquity = capital;
                        s.entryZ = order.z;
                        s.entryDate = a.date;
                        s.signalDate = order.signal;
                        s.entryIndex = int(i);
                        s.direction = order.direction;
                        s.book.fill(true, order.qa, a.open, c);
                        s.book.fill(false, order.qb, b.open, c);
                        s.events.push_back({{"date", a.date},
                                            {"type", "entry"},
                                            {"signal_date", order.signal},
                                            {"reason", "spread_deviation"},
                                            {"z", order.z},
                                            {"delta_a", order.qa},
                                            {"delta_b", order.qb},
                                            {"open_a", a.open},
                                            {"open_b", b.open},
                                            {"cash", s.book.cash}});
                    }
                }
            }
            double value = s.book.equity(a.close, b.close),
                   exposure = s.book.gross(a.close, b.close);
            require(value > 0, "Sleeve bankrupt: " + s.id + " " + a.date);
            double z =
                s.model.sigma > 0 ? zscore(s.model, assetA.logTotal[i], assetB.logTotal[i]) : 0;
            if (i < last) {
                if (s.direction) {
                    std::string reason;
                    bool risk = false;
                    if (value <= c.maintenance * exposure) {
                        reason = "maintenance_breach";
                        risk = true;
                    } else if (value / s.entryEquity - 1 <= -c.pairStop) {
                        reason = "capital_stop";
                        risk = true;
                    } else if (std::abs(z) >= c.stop) {
                        reason = "spread_stop";
                        risk = true;
                    } else if (s.direction * z >= -c.exit)
                        reason = "mean_reversion";
                    else if (int(i) - s.entryIndex + 1 >= c.maxHold)
                        reason = "time_stop";
                    if (!reason.empty() && (s.pending.due < 0 || risk)) {
                        int due = int(i) + (risk ? 1 : c.delay);
                        s.pending = {due, 0, 0, 0, z, a.date, reason};
                        if (risk)
                            s.blocked = true;
                    }
                } else if (s.pending.due < 0 && s.model.eligible && !s.blocked &&
                           std::abs(z) >= c.entry && std::abs(z) < c.stop) {
                    int direction = z > 0 ? -1 : 1;
                    double dollars = c.gross * value / (1 + s.model.beta);
                    s.pending = {int(i) + c.delay,
                                 direction,
                                 direction * dollars / a.close,
                                 -direction * s.model.beta * dollars / b.close,
                                 z,
                                 a.date,
                                 "spread_deviation"};
                }
            }
            if (s.direction)
                ++active;
            eq += value;
            gross += exposure;
            net += s.book.qa * a.close + s.book.qb * b.close;
            s.daily.push_back(
                {{"date", a.date},
                 {"equity", value},
                 {"cash", s.book.cash},
                 {"qty_a", s.book.qa},
                 {"qty_b", s.book.qb},
                 {"gross", exposure},
                 {"z", s.model.sigma > 0 ? finite(z) : J(nullptr)},
                 {"eligible", s.model.eligible},
                 {"position", s.direction},
                 {"index_a", std::exp(assetA.logTotal[i] - assetA.logTotal[first]) * 100},
                 {"index_b", std::exp(assetB.logTotal[i] - assetB.logTotal[first]) * 100}});
        }
        const auto &bar = data.assets.at(c.benchmark).bars[i];
        Bar dummy = bar;
        dummy.split = 1;
        dummy.dividend = 0;
        benchmark.actions(bar, dummy);
        if (i == first) {
            double q = (benchmark.cash - c.commission) / (bar.open * (1 + c.slipBps / 10000));
            benchmark.fill(true, q, bar.open, c);
        }
        if (i == last)
            benchmark.fill(true, -benchmark.qa, bar.open, c);
        double bench = benchmark.equity(bar.close, 0), basketValue = 0;
        for (auto &item : basket) {
            const auto &b = data.assets.at(item.first).bars[i];
            auto &account = item.second;
            account.actions(b, dummy);
            if (i == first) {
                double q = (account.cash - c.commission) / (b.open * (1 + c.slipBps / 10000));
                account.fill(true, q, b.open, c);
            }
            if (i == last)
                account.fill(true, -account.qa, b.open, c);
            basketValue += account.equity(b.close, 0);
        }
        allFinite = allFinite && std::isfinite(eq);
        require(allFinite, "Nonfinite account value");
        dates.push_back(bar.date);
        equity.push_back(eq);
        spyEquity.push_back(bench);
        basketEquity.push_back(basketValue);
        peak = std::max(peak, eq);
        spyPeak = std::max(spyPeak, bench);
        basketPeak = std::max(basketPeak, basketValue);
        if (active)
            ++activeDays;
        daily.push_back({{"date", bar.date},
                         {"equity", eq},
                         {"drawdown", eq / peak - 1},
                         {"benchmark", bench},
                         {"benchmark_drawdown", bench / spyPeak - 1},
                         {"basket", basketValue},
                         {"basket_drawdown", basketValue / basketPeak - 1},
                         {"gross_exposure", gross / eq},
                         {"net_exposure", net / eq},
                         {"active_pairs", active}});
    }
    J metrics = summaryMetrics(equity, dates), pairResults = J::array(), trades = J::array();
    double fees = 0, slip = 0, borrow = 0, interest = 0, div = 0, turnover = 0, pnl = 0;
    int wins = 0;
    for (auto &s : sleeves) {
        std::vector<double> e = {initial};
        for (const auto &row : s.daily)
            e.push_back(row["equity"].get<double>());
        J m = summaryMetrics(e, dates);
        double contribution = s.book.cash - initial;
        int eligible = 0;
        for (const auto &f : s.folds)
            if (f["eligible"].get<bool>())
                ++eligible;
        m["pnl"] = contribution;
        m["trades"] = s.trades.size();
        m["eligible_windows"] = eligible;
        m["windows"] = s.folds.size();
        m["fees"] = s.book.fees;
        m["slippage"] = s.book.slippage;
        m["borrow"] = s.book.borrow;
        m["interest"] = s.book.interest;
        m["net_dividends"] = s.book.dividends;
        pairResults.push_back({{"id", s.id},
                               {"a", s.a},
                               {"b", s.b},
                               {"metrics", m},
                               {"folds", s.folds},
                               {"daily", s.daily},
                               {"trades", s.trades},
                               {"events", s.events}});
        for (const auto &t : s.trades) {
            trades.push_back(t);
            double p = t["net_pnl"].get<double>();
            pnl += p;
            if (p > 0)
                ++wins;
        }
        fees += s.book.fees;
        slip += s.book.slippage;
        borrow += s.book.borrow;
        interest += s.book.interest;
        div += s.book.dividends;
        turnover += s.book.turnover;
    }
    require(std::abs(pnl - (equity.back() - c.capital)) < 1e-6 * c.capital,
            "Trade P&L does not reconcile with account equity");
    metrics["trades"] = trades.size();
    metrics["win_rate"] = trades.empty() ? J(nullptr) : J(double(wins) / double(trades.size()));
    metrics["invested_fraction"] = double(activeDays) / double(daily.size());
    metrics["fees"] = fees;
    metrics["slippage"] = slip;
    metrics["borrow"] = borrow;
    metrics["interest"] = interest;
    metrics["net_dividends"] = div;
    metrics["turnover"] = turnover;
    J annual = J::array();
    std::map<std::string, std::pair<double, double>> years;
    double previous = c.capital;
    for (const auto &row : daily) {
        std::string year = row["date"].get<std::string>().substr(0, 4);
        if (!years.count(year))
            years[year] = {previous, 0};
        years[year].second = row["equity"].get<double>();
        previous = years[year].second;
    }
    for (auto &y : years)
        annual.push_back({{"year", y.first}, {"return", y.second.second / y.second.first - 1}});
    return {{"name", name},
            {"first_signal_date", data.dates[first]},
            {"last_date", data.dates[last]},
            {"initial_date", dates[0]},
            {"metrics", metrics},
            {"benchmark_metrics", summaryMetrics(spyEquity, dates)},
            {"basket_metrics", summaryMetrics(basketEquity, dates)},
            {"daily", daily},
            {"pairs", pairResults},
            {"trades", trades},
            {"annual", annual}};
}
}                
