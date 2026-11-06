
import argparse, csv, json
from datetime import date
from pathlib import Path
import numpy as np
import statsmodels.api as sm
from statsmodels.tsa.stattools import coint


def load(path):
    with path.open() as f:
        return list(csv.DictReader(f))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--data", type=Path, default=Path("data/real"))
    p.add_argument(
        "--results", type=Path, default=Path("results/baseline/results.json")
    )
    a = p.parse_args()
    r = json.loads(a.results.read_text())
    c = r["config"]
    cache = {}
    max_stat = 0.0
    max_equity = 0.0
    windows = 0
    daily_count = 0
    for pair in r["baseline"]["pairs"]:
        for symbol in [pair["a"], pair["b"]]:
            if symbol not in cache:
                rows = load(a.data / (symbol + ".csv"))
                fields = {
                    k: np.array([float(row[k]) for row in rows])
                    for k in ["open", "close", "dividend", "split"]
                }
                fields["dates"] = [row["date"] for row in rows]
                ratio = (
                    fields["split"][1:]
                    * (fields["close"][1:] + fields["dividend"][1:])
                    / fields["close"][:-1]
                )
                fields["log"] = np.r_[0.0, np.cumsum(np.log(ratio))]
                cache[symbol] = fields
        ay, bx = cache[pair["a"]], cache[pair["b"]]
        dates = ay["dates"]
        indices = {d: i for i, d in enumerate(dates)}
        for fold in pair["folds"]:
            start, stop = indices[fold["train_start"]], indices[fold["train_end"]] + 1
            y, x = ay["log"][start:stop], bx["log"][start:stop]
            fit = sm.OLS(y, sm.add_constant(x)).fit()
            stat, _, crit = coint(y, x, trend="c", maxlag=c["adf_lag"], autolag=None)
            errors = [
                abs(fit.params[0] - fold["alpha"]),
                abs(fit.params[1] - fold["beta"]),
                abs(stat - fold["adf"]),
                abs(crit[0] - fold["critical"]),
                abs(np.std(fit.resid, ddof=1) - fold["sigma"]),
            ]
            max_stat = max(max_stat, *errors)
            assert max(errors) < 1e-8, errors
            windows += 1

        equity = c["capital"] / len(c["pairs"])
        cash = equity
        qa = qb = 0.0
        events = {}
        for event in pair["events"]:
            events.setdefault(event["date"], []).append(event)
        for row in pair["daily"]:
            i = indices[row["date"]]
            prev_a, prev_b = ay["close"][i - 1], bx["close"][i - 1]
            days = (
                date.fromisoformat(dates[i]) - date.fromisoformat(dates[i - 1])
            ).days
            short = max(0, -qa * prev_a) + max(0, -qb * prev_b)
            cost = (
                short * c["borrow_rate"] * days / 365
                + max(0, short - cash) * c["margin_rate"] * days / 365
            )
            price_pnl = qa * (ay["split"][i] * ay["close"][i] - prev_a) + qb * (
                bx["split"][i] * bx["close"][i] - prev_b
            )
            qa *= ay["split"][i]
            qb *= bx["split"][i]
            div = qa * ay["dividend"][i] + qb * bx["dividend"][i]
            execution_pnl = 0
            for ev in events.get(row["date"], []):
                if ev["type"] == "rejected_entry":
                    continue
                da, db = ev["delta_a"], ev["delta_b"]
                execution_pnl += da * (ay["close"][i] - ev["open_a"]) + db * (
                    bx["close"][i] - ev["open_b"]
                )
                cost += (abs(da) * ev["open_a"] + abs(db) * ev["open_b"]) * c[
                    "slippage_bps"
                ] / 10000 + 2 * c["commission"]
                qa += da
                qb += db
            equity += price_pnl + div + execution_pnl - cost
            cash = equity - qa * ay["close"][i] - qb * bx["close"][i]
            err = abs(equity - row["equity"])
            max_equity = max(max_equity, err)
            assert err < 1e-6, (row["date"], err)
            daily_count += 1
    print(
        json.dumps(
            {
                "training_windows_checked": windows,
                "daily_sleeve_values_reconciled": daily_count,
                "maximum_statistical_absolute_error": max_stat,
                "maximum_equity_absolute_error_dollars": max_equity,
                "status": "passed",
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
