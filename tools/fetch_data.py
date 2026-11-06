

import argparse, csv, hashlib, json, math, sys, time, urllib.request, urllib.parse
from datetime import date, datetime, timezone
from pathlib import Path

SYMBOLS = ["KO", "PEP", "XOM", "CVX", "V", "MA", "JPM", "BAC", "SPY"]


def fnv(data):
    h = 14695981039346656037
    for b in data:
        h = ((h ^ b) * 1099511628211) & ((1 << 64) - 1)
    return f"{h:016x}"


def fetch_chart(symbol, start, end):

    since = int(datetime.combine(start, datetime.min.time(), timezone.utc).timestamp())
    until = int(
        datetime.combine(date.today(), datetime.min.time(), timezone.utc).timestamp()
    )
    params = urllib.parse.urlencode(
        {
            "period1": since,
            "period2": until,
            "interval": "1d",
            "events": "div,splits",
            "includeAdjustedClose": "true",
        }
    )
    url = "https://query1.finance.yahoo.com/v8/finance/chart/" + symbol + "?" + params
    request = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(request, timeout=60) as r:
        payload = json.load(r)
    result = payload["chart"]["result"][0]
    if (
        result["meta"].get("currency") != "USD"
        or result["meta"].get("dataGranularity") != "1d"
    ):
        raise ValueError("Currency or daily granularity mismatch")
    quote = result["indicators"]["quote"][0]
    events = result.get("events", {})

    def session(t):
        return datetime.fromtimestamp(int(t), timezone.utc).date()

    splits = {
        session(v["date"]): float(v["numerator"]) / float(v["denominator"])
        for v in events.get("splits", {}).values()
    }
    dividends = {}
    for key in ["dividends", "capitalGains"]:
        for v in events.get(key, {}).values():
            dividends[session(v["date"])] = dividends.get(
                session(v["date"]), 0
            ) + float(v["amount"])
    dates = [session(t) for t in result["timestamp"]]
    if dates != sorted(set(dates)):
        raise ValueError("Unordered/duplicate timestamps")
    if any(d not in dates for d in list(splits) + list(dividends) if start <= d < end):
        raise ValueError("Corporate action has no matching price session")
    factor = 1.0
    rows = []
    for i in range(len(dates) - 1, -1, -1):
        d = dates[i]
        ratio = splits.get(d, 1.0)
        if not math.isfinite(ratio) or ratio <= 0:
            raise ValueError("Invalid split")
        if start <= d < end:
            values = [
                float(quote[k][i]) * factor for k in ["open", "high", "low", "close"]
            ]
            div = dividends.get(d, 0) * factor
            if (
                not all(math.isfinite(x) and x > 0 for x in values)
                or not math.isfinite(div)
                or div < 0
            ):
                raise ValueError("Invalid prices/actions")
            o, h, l, c = values
            if l > min(o, c) + 1e-7 or h + 1e-7 < max(o, c):
                raise ValueError("Inconsistent OHLC")
            rows.append([d.isoformat(), *values, div, ratio])
        factor *= ratio
    rows.reverse()
    if len(rows) < 600:
        raise ValueError("Not enough daily sessions")
    details = {
        "rows": len(rows),
        "first": rows[0][0],
        "last": rows[-1][0],
        "download_start": str(start),
        "download_end_exclusive": str(date.today()),
        "splits_in_download": {str(k): v for k, v in splits.items()},
        "granularity": "1d",
    }
    return rows, details


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out", type=Path, default=Path("data/real"))
    p.add_argument("--start", type=date.fromisoformat, default=date(2010, 1, 1))
    p.add_argument("--end", type=date.fromisoformat, default=date(2024, 1, 1))
    a = p.parse_args()
    if a.out.exists():
        p.error(
            "Output already exists. Choose a fresh directory to preserve provenance."
        )
    if not a.start < a.end <= date.today():
        p.error("Use start < end <= today; incomplete current sessions excluded.")
    all_rows = {}
    manifest = {
        "schema": "pairlab.data.v1",
        "synthetic": False,
        "source": "Yahoo Finance public daily chart endpoint",
        "retrieved_utc": datetime.now(timezone.utc).isoformat(),
        "price_basis": "reconstructed_as_traded",
        "start": str(a.start),
        "end_exclusive": str(a.end),
        "files": {},
        "limitations": [
            "Current surviving companies selected retrospectively; not a point-in-time universe.",
            "Provider history can be revised; corporate action reconstruction depends on complete reported splits.",
            "No historical borrow availability or intraday liquidity data.",
            "For personal research; provider data rights are separate from code license.",
        ],
    }
    for symbol in SYMBOLS:
        print("Downloading", symbol, flush=True)
        rows, details = fetch_chart(symbol, a.start, a.end)
        all_rows[symbol] = rows
        manifest["files"][symbol] = details
    calendar = [r[0] for r in all_rows["SPY"]]
    for symbol, rows in all_rows.items():
        if [r[0] for r in rows] != calendar:
            raise ValueError(
                f"{symbol}: sessions differ from SPY. No silent filling or dropping."
            )
    a.out.mkdir(parents=True)
    for symbol, rows in all_rows.items():
        path = a.out / (symbol + ".csv")
        with path.open("x", newline="", encoding="utf-8") as f:
            w = csv.writer(f, lineterminator="\n")
            w.writerow(["date", "open", "high", "low", "close", "dividend", "split"])
            w.writerows(rows)
        data = path.read_bytes()
        manifest["files"][symbol].update(
            sha256=hashlib.sha256(data).hexdigest(), fnv1a64=fnv(data)
        )
    (a.out / "provenance.json").write_text(
        json.dumps(manifest, indent=2), encoding="utf-8"
    )
    print("Saved", len(calendar), "sessions for", len(all_rows), "symbols to", a.out)


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print("Download failed:", e, file=sys.stderr)
        sys.exit(1)
