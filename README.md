# PairLab

C++17 pairs-trading research with walk-forward statistical screening, long/short accounting, execution costs and reproducible reports.

# Live-data update

**For the new live screen, read START_LIVE.md.** After rebuilding, run
RUN_LIVE.cmd. No brokerage account, API key or additional Python packages are
required. RUN_LIVE_PAPER.cmd adds an optional intraday shadow ledger;
RUN_LIVE_DEMO.cmd uses invented prices offline. Public quotes may be delayed;
stale data and closed sessions cannot produce current candidates.

One alternative strategy and its full historical report are included. See
OPEN_ALTERNATIVE.cmd and docs/STRATEGY_COMPARISON.md. It remains experimental.

# PairLab

A C++17 pairs-trading research engine with a self-contained interactive report.
The project asks whether a relationship fitted on earlier stock prices survives
later trading, execution costs, short borrow and regime changes.

**Start by opening `docs/index.html` in your browser**, or double-click
`OPEN_REPORT.cmd`. The included report uses real historical data, not a synthetic
price demonstration. No installation is required to read it.

To compile and rerun on Windows, see **START_HERE.md**. To learn the reasoning,
read **docs/EXPLAINING_THE_PROJECT.md** and **docs/METHOD.md**.

## Included experiment

- Four predefined, disjoint pairs: KO/PEP, XOM/CVX, V/MA, JPM/BAC.
- 3,522 synchronized daily sessions for nine symbols, 2010–2023; includes SPY.
- 504-session rolling formation window; 63-session forward trading window.
- Engle–Granger residual screen using the appropriate two-series 1% critical
  value, positive hedge ratio, half-life and stability checks.
- Frozen models within each window; closing signals execute at later opens.
- Equal starting capital sleeves, long and short share accounting, splits,
  dividends, fees, adverse slippage, borrow, financing and simple capital limits.
- SPY and equal-weight buy-and-hold comparisons, zero-yield cash comparison,
  annual results, attribution and four predefined execution/borrow stresses.
- Detailed model, order, trade and daily holdings audit exports.

The fixed baseline returned **−1.92%**, with **2.25% maximum drawdown** and
**15 completed pair trades** from January 2012 through December 2023. Only 12 of
192 pair-windows passed every screen. It did not demonstrate a profitable edge.
The result is retained without tuning rules to improve its appearance.

## Layout

| Path | Purpose |
|---|---|
| `src/common.hpp` | Configuration, validation, dates and file utilities |
| `src/data.hpp` | Strict CSV loader and causal total-return indices |
| `src/statistics.hpp` | QR regression, residual ADF, model screening |
| `src/engine.hpp` | Walk-forward scheduling and long/short accounting |
| `src/main.cpp` | CLI, stress scenarios, audit and report export |
| `config/default.json` | Fixed baseline assumptions |
| `data/real/` | Archived prices/actions and provider provenance |
| `tools/fetch_data.py` | Optional Python standard-library downloader |
| `tests/` | C++ engine tests and Python importer tests |
| `web/report.html` | Report template used by the C++ executable |
| `docs/index.html` | Ready-to-open baseline report |
| `results/baseline/` | Complete reproducible run and audits |

The C++ engine has no external runtime libraries. The vendored JSON header keeps
its original MIT license. Python is needed only for downloading fresh data and
optional independent validation; no Python is called by the backtester.

## Research limitations

The universe was selected retrospectively from surviving companies. Prices are
revised vendor history, not a point-in-time archive. Borrow availability is
assumed; constant borrow/financing rates are scenarios, not historical quotes.
Simultaneous opening fills, fractional shares and daily risk checks are simplified
execution assumptions. Statistical rejection is not proof of future reversion.

Dates from 2024 onward were not scored. The download process requests later data
only to undo later split adjustments correctly; no later observations are placed
in the shipped strategy CSVs. This is historical development, not a preregistered
or untouched live experiment.

Code licensing and data rights are separate. The code is MIT-licensed; Yahoo
Finance data remains subject to its provider terms. Do not describe vendor data
as public-domain or independently audited.

## Diagnostic extension

The report now includes a Diagnostics tab with overlapping filter rejection counts,
exit-reason P&L, trade outcome statistics and a same-path cost add-back. See
`docs/DIAGNOSTICS.md`. Strategy settings and baseline results remain fixed.
