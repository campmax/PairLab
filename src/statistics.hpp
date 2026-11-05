#pragma once
#include "data.hpp"
namespace pl {
struct Regression {
    std::vector<double> coef, se, residual;
    double r2 = 0;
};
                                                                             
inline Regression ols(const std::vector<std::vector<double>> &x, const std::vector<double> &y) {
    size_t n = x.size(), k = x.empty() ? 0 : x[0].size();
    require(n == y.size() && n > k && k > 0, "Regression dimensions");
    std::vector<std::vector<double>> q(k, std::vector<double>(n)), r(k, std::vector<double>(k));
    for (size_t j = 0; j < k; ++j) {
        for (size_t i = 0; i < n; ++i) {
            require(x[i].size() == k, "Ragged regression");
            q[j][i] = x[i][j];
        }
        for (int pass = 0; pass < 2; ++pass)
            for (size_t a = 0; a < j; ++a) {
                double dot = std::inner_product(q[a].begin(), q[a].end(), q[j].begin(), 0.0);
                r[a][j] += dot;
                for (size_t i = 0; i < n; ++i)
                    q[j][i] -= dot * q[a][i];
            }
        r[j][j] = std::sqrt(std::inner_product(q[j].begin(), q[j].end(), q[j].begin(), 0.0));
        require(r[j][j] > 1e-10, "Singular/near-singular regression");
        for (auto &v : q[j])
            v /= r[j][j];
    }
    auto solve = [&](std::vector<double> v) {
        for (int i = int(k) - 1; i >= 0; --i) {
            for (size_t j = size_t(i) + 1; j < k; ++j)
                v[size_t(i)] -= r[size_t(i)][j] * v[j];
            v[size_t(i)] /= r[size_t(i)][size_t(i)];
        }
        return v;
    };
    Regression result;
    std::vector<double> rhs(k);
    for (size_t j = 0; j < k; ++j)
        rhs[j] = std::inner_product(q[j].begin(), q[j].end(), y.begin(), 0.0);
    result.coef = solve(rhs);
    double ss = 0, total = 0, ym = mean(y);
    for (size_t i = 0; i < n; ++i) {
        double e = y[i] - std::inner_product(x[i].begin(), x[i].end(), result.coef.begin(), 0.0);
        result.residual.push_back(e);
        ss += e * e;
        total += (y[i] - ym) * (y[i] - ym);
    }
    result.r2 = total > 0 ? 1 - ss / total : 0;
    result.se.assign(k, 0);
    for (size_t j = 0; j < k; ++j) {
        std::vector<double> unit(k);
        unit[j] = 1;
        auto inv = solve(unit);
        for (size_t i = 0; i < k; ++i)
            result.se[i] += inv[i] * inv[i];
    }
    for (auto &v : result.se)
        v = std::sqrt(v * ss / double(n - k));
    return result;
}
inline Regression line(const std::vector<double> &x, const std::vector<double> &y) {
    std::vector<std::vector<double>> design;
    for (double v : x)
        design.push_back({1, v});
    return ols(design, y);
}
inline double adfStatistic(const std::vector<double> &residual, int lag) {
    require(residual.size() > size_t(lag + 5), "Insufficient ADF sample");
    std::vector<std::vector<double>> x;
    std::vector<double> y;
    for (size_t t = size_t(lag) + 1; t < residual.size(); ++t) {
        std::vector<double> row = {residual[t - 1]};
        for (int j = 1; j <= lag; ++j)
            row.push_back(residual[t - size_t(j)] - residual[t - size_t(j) - 1]);
        x.push_back(row);
        y.push_back(residual[t] - residual[t - 1]);
    }
    auto r = ols(x, y);
    require(r.se[0] > 1e-12, "Degenerate ADF residual variance");
    return r.coef[0] / r.se[0];
}
                                                                                
/* Numerical coefficients from statsmodels (BSD license in third_party). */
inline double egCritical(size_t observations) {
    double n = double(observations - 1);
    return -3.89644 - 10.9519 / n - 33.527 / (n * n);
}
struct Model {
    double alpha = 0, beta = 0, sigma = 0, adf = 0, critical = 0, half = 0, drift = 0,
           correlation = 0;
    bool eligible = false;
    std::vector<std::string> reasons;
};
inline Model fitModel(const Asset &a, const Asset &b, size_t begin, size_t end, const Config &c) {
    Model m;
    require(end <= a.logTotal.size() && end <= b.logTotal.size() && end > begin + 20,
            "Invalid training range");
    try {
        std::vector<double> y(a.logTotal.begin() + long(begin), a.logTotal.begin() + long(end)),
            x(b.logTotal.begin() + long(begin), b.logTotal.begin() + long(end));
        auto reg = line(x, y);
        m.alpha = reg.coef[0];
        m.beta = reg.coef[1];
        m.sigma = std::sqrt(variance(reg.residual));
        m.adf = adfStatistic(reg.residual, c.adfLag);
        m.critical = egCritical(y.size());
        std::vector<double> prior(reg.residual.begin(), reg.residual.end() - 1),
            next(reg.residual.begin() + 1, reg.residual.end());
        double phi = line(prior, next).coef[1];
        m.half = phi > 0 && phi < 1 ? -std::log(2.0) / std::log(phi) : 0;
        size_t mid = y.size() / 2;
        double b1 = line({x.begin(), x.begin() + long(mid)}, {y.begin(), y.begin() + long(mid)})
                        .coef[1],
               b2 =
                   line({x.begin() + long(mid), x.end()}, {y.begin() + long(mid), y.end()}).coef[1];
        m.drift = std::abs(b1 - b2) / std::max(.01, std::abs(m.beta));
        std::vector<double> rx, ry;
        for (size_t i = 1; i < x.size(); ++i) {
            rx.push_back(x[i] - x[i - 1]);
            ry.push_back(y[i] - y[i - 1]);
        }
        double mx = mean(rx), my = mean(ry), cov = 0;
        for (size_t i = 0; i < rx.size(); ++i)
            cov += (rx[i] - mx) * (ry[i] - my);
        m.correlation = cov / double(rx.size() - 1) / std::sqrt(variance(rx) * variance(ry));
        if (m.adf >= m.critical)
            m.reasons.push_back("Residual unit-root screen failed (1%)");
        if (m.beta < c.minBeta || m.beta > c.maxBeta)
            m.reasons.push_back("Hedge ratio outside allowed positive range");
        if (m.half < c.minHalf || m.half > c.maxHalf)
            m.reasons.push_back("Reversion half-life outside allowed range");
        if (m.drift > c.maxBetaDrift)
            m.reasons.push_back("Hedge ratio unstable across training halves");
        if (m.sigma < c.minSigma)
            m.reasons.push_back("Spread variation too small");
        m.eligible = m.reasons.empty();
    } catch (const std::exception &e) {
        m.reasons.push_back(e.what());
    }
    return m;
}
inline J modelJson(const Model &m) {
    return {{"alpha", m.alpha},
            {"beta", m.beta},
            {"sigma", m.sigma},
            {"adf", finite(m.adf)},
            {"critical", m.critical},
            {"half_life", finite(m.half)},
            {"beta_drift", finite(m.drift)},
            {"correlation", finite(m.correlation)},
            {"eligible", m.eligible},
            {"reasons", m.reasons}};
}
inline double zscore(const Model &m, double y, double x) {
    require(m.sigma > 0, "Zero spread scale");
    return (y - m.alpha - m.beta * x) / m.sigma;
}
}                
