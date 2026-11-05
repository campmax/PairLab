#include "engine.hpp"
#include <chrono>
using namespace pl;
inline std::string jsonForHtml(const J &j) {
    std::string in = j.dump(), out;
    for (char c : in) {
        if (c == '<')
            out += "\\u003c";
        else if (c == '>')
            out += "\\u003e";
        else if (c == '&')
            out += "\\u0026";
        else
            out += c;
    }
    return out;
}
inline void exportCSV(const fs::path &path, const J &rows, const std::vector<std::string> &keys) {
    std::ostringstream out;
    for (size_t i = 0; i < keys.size(); ++i)
        out << (i ? "," : "") << keys[i];
    out << '\n';
    for (const auto &r : rows) {
        for (size_t i = 0; i < keys.size(); ++i) {
            if (i)
                out << ',';
            if (!r.contains(keys[i]) || r[keys[i]].is_null())
                continue;
            if (r[keys[i]].is_string()) {
                std::string s = r[keys[i]].get<std::string>();
                out << '"';
                for (char c : s) {
                    if (c == '"')
                        out << '"';
                    out << c;
                }
                out << '"';
            } else
                out << r[keys[i]].dump();
        }
        out << '\n';
    }
    writeText(path, out.str());
}
int main(int argc, char **argv) {
    try {
        std::map<std::string, std::string> options;
        bool stress = false, holdout = false;
        for (int i = 1; i < argc; ++i) {
            std::string key = argv[i];
            if (key == "--help") {
                std::cout << "PairLab C++17\n--data data/real --config config/default.json --out "
                             "runs/name [--stress] [--allow-holdout]\n";
                return 0;
            }
            if (key == "--stress") {
                stress = true;
                continue;
            }
            if (key == "--allow-holdout") {
                holdout = true;
                continue;
            }
            require(key == "--data" || key == "--config" || key == "--out" || key == "--template",
                    "Unknown argument: " + key);
            require(i + 1 < argc && !options.count(key), "Missing/duplicate argument");
            options[key] = argv[++i];
        }
        auto get = [&](const std::string &k, const std::string &fallback) {
            return options.count(k) ? options.at(k) : fallback;
        };
        fs::path configPath = get("--config", "config/default.json"),
                 dataPath = get("--data", "data/real"),
                 out = get("--out",
                           "runs/run_" +
                               std::to_string(
                                   std::chrono::system_clock::now().time_since_epoch().count()));
        J configJson = readJson(configPath);
        Config c = parseConfig(configJson);
        require(c.end <= c.holdout || holdout,
                "Reserved period requires explicit --allow-holdout. Freeze rules first.");
        require(!fs::exists(out), "Output already exists; choose a new folder");
        auto dataset = loadData(dataPath, c);
        std::cout << "PairLab | "
                  << (dataset.provenance.value("synthetic", false) ? "SYNTHETIC"
                                                                   : "REAL HISTORICAL DATA")
                  << "\nRunning walk-forward research...\n";
        J base = runBacktest(dataset, c, "Baseline"), scenarios = J::array();
        auto scenario = [&](const std::string &name, Config alternative) {
            J run = runBacktest(dataset, alternative, name);
            scenarios.push_back(
                {{"name", name}, {"metrics", run["metrics"]}, {"daily", run["daily"]}});
        };
        if (stress) {
            Config x = c;
            x.slipBps *= 2;
            x.commission *= 2;
            scenario("Double execution costs", x);
            x = c;
            x.borrow = .10;
            scenario("10% annual borrow", x);
            x = c;
            x.delay = 2;
            scenario("One extra session delay", x);
            x = c;
            x.slipBps *= 2;
            x.commission *= 2;
            x.borrow = .10;
            x.delay = 2;
            scenario("Combined stress", x);
        }
        J result = {{"schema", "pairlab.report.v1"},
                    {"config", configJson},
                    {"data_provenance", dataset.provenance},
                    {"config_fingerprint_fnv1a64", fingerprint(configJson.dump())},
                    {"holdout_accessed", c.end > c.holdout},
                    {"baseline", base},
                    {"scenarios", scenarios}};
        std::string html = readText(get("--template", "web/report.html"));
        std::string token = "__PAIRLAB_DATA__";
        auto pos = html.find(token);
        require(pos != std::string::npos, "Report template token missing");
        html.replace(pos, token.size(), jsonForHtml(result));
        fs::create_directories(out);
        writeText(out / "report.html", html);
        writeText(out / "results.json", result.dump(2));
        writeText(out / "run_config.json", configJson.dump(2));
        writeText(out / "data_provenance.json", dataset.provenance.dump(2));
        exportCSV(out / "equity.csv", base["daily"],
                  {"date", "equity", "drawdown", "benchmark", "basket", "gross_exposure",
                   "net_exposure", "active_pairs"});
        exportCSV(out / "trades.csv", base["trades"],
                  {"pair", "signal_date", "entry_date", "exit_signal_date", "exit_date",
                   "direction", "entry_z", "held_sessions", "reason", "net_pnl", "sleeve_return"});
        for (const auto &p : base["pairs"]) {
            std::string id = p["a"].get<std::string>() + "_" + p["b"].get<std::string>();
            writeText(out / (id + "_audit.json"), p.dump(2));
            exportCSV(out / (id + "_folds.csv"), p["folds"],
                      {"train_start", "train_end", "trade_start", "trade_end", "eligible", "alpha",
                       "beta", "sigma", "adf", "critical", "half_life", "beta_drift",
                       "correlation"});
        }
        std::cout << "Return: " << base["metrics"]["return"].get<double>() * 100
                  << "% | Trades: " << base["metrics"]["trades"]
                  << "\nReport: " << fs::absolute(out / "report.html").string() << "\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}
