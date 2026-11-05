# Validation record

Validated on 2026-10-03 in a Linux/GCC environment.

## Passed

- C++17 release compilation with GCC 13, `-O2 -Wall -Wextra -Wpedantic`.
- **29 C++ tests**, including OLS, singular regression, critical-value selection,
  cash/short liabilities, adverse execution costs, split invariance, dividends,
  weekend borrow, financing collateral, drawdown, signal timing, delayed execution,
  failed-screen cash behavior, risk exits, final liquidation, deterministic runs,
  trade reconciliation and invariance of earlier results to future-data changes.
- **6 Python importer tests** covering later split reconstruction before cropping,
  split-date units, invalid granularity, missing prices, duplicate dates and a
  known fingerprint value. These tests require only the Python standard library.
- **192 training windows** checked independently against statsmodels OLS and
  `coint(..., trend='c', maxlag=1, autolag=None)`. Maximum absolute discrepancy
  across coefficients, residual standard deviations, residual statistics and
  critical values: less than 3e-13.
- **12,072 daily sleeve values** independently reconciled using a signed-share
  price-P&L identity plus distributions and costs. Maximum absolute account-value
  discrepancy: less than $1.5e-11. Full figures: reference_validation.json.
- Full baseline and four predefined stress scenarios completed on real data.
- Report JavaScript/DOM checks: five SVG charts, pair changes, all training windows,
  empty eligible-window state, trade filtering and sorting, benchmark switching,
  tab navigation, exact configuration display and no JavaScript errors.

The independent calculations use the same declared market data and modeling
assumptions. Agreement verifies implementation consistency, not that those
assumptions are economically complete or that the input data is error-free.

## Not verified here

- Native Windows/MSVC compilation, Windows batch-file execution and browser launch.
  Run build_windows.cmd on your computer; it compiles and runs the C++ tests.
- Full browser layout/screenshot review. DOM checks exercise interaction and SVG
  generation but do not establish pixel-level rendering on every screen size.
- CMake generator execution; direct compiler commands were used.
- Actual trading fills, historical stock-loan availability/rates, recalls,
  broker-specific margin rules or intraday stop execution.
- Independent corporate-action reconciliation against an institutional vendor,
  point-in-time universe reconstruction, predictive profitability or live results.

## Reproduce checks

From the project root after a C++ build:

```powershell
.\bin\pairlab_tests.exe
py tests\test_importer.py
```

Optional independent statistics/accounting verification:

```powershell
py -m pip install numpy statsmodels
py tools\validate_reference.py --data data\real --results results\baseline\results.json
```

Optional report DOM check (requires Node and jsdom):

```sh
npm install jsdom
node tests/report_dom.cjs results/baseline/report.html
```

## Diagnostic extension checks

Report DOM tests also verify diagnostic tab navigation, pair/window counts,
exit-reason grouping and the formatted cost add-back against baseline metrics.
No engine, market data, or strategy configuration was modified. Visual-rendering
and native Windows limitations above still apply.

## Live-data extension

- Original 29 C++ tests rerun and passed.
- 13 new C++ live-scoring tests passed: fresh candidates, stale/future/closed/skewed
  quotes, isolated provider failures, invalid prices, calendar mismatches, training
  exclusion, causal quote changes, entry stop thresholds, share basis and demo labeling.
- 13 Python adapter/paper tests passed: exclusion of current session, malformed
  history, timestamp confirmation, duplicate and single-leg quotes, stale resets,
  convergence/session exits, accounting, overnight suspension and reentry blocking.
- Existing six importer tests retained; total automated C++/Python tests: 61.
- Live dashboard DOM checks passed: tables, pair selection, evolving charts,
  stale/error/disconnection gating and text escaping.
- Local HTTP integration checks cover refreshes, no-store responses, Host guard,
  refusal to serve arbitrary project files, paper journals and stopped status.
- Real public-data collection succeeded for all eight stocks. The captured session
  was closed; all four pair signals were correctly blocked. Archive:
  examples/live_capture/scored.json. It is NOT a current market recommendation.
- Seven-cycle synthetic live integration generated four simulated entries and
  four exits. This validates software behavior, not real execution or profitability.
- One predefined alternative historical configuration and all four stresses ran
  successfully. Its report passed the existing DOM checks. No holdout was scored.

Live quotes during an open exchange session, native Windows/MSVC operation,
full browser screenshots, feed uptime and actual brokerage execution remain
unverified. There is no brokerage API or order-submission code in this release.

Additional commands:

```
py tests/test_live.py
bin/live_tests.exe tests/live_fixture.json
```

Optional Node test: `node tests/live_dom.cjs examples/live_capture/scored.json`.
It substitutes fresh synthetic timestamps to test UI behavior; the archived quote
file itself remains explicitly non-current.
