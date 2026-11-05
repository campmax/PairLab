# PairLab live watch — start here

No brokerage account, API key, or additional Python packages are required.
The app reads public Yahoo chart data and calculates pair scores locally.
This is automatically refreshed indicative data; it is not a guaranteed real-time
exchange feed. Each quote's original timestamp is shown and checked.

## Windows: three steps

1. Extract the entire updated ZIP into a new folder.
2. Double-click **build_windows.cmd**. Wait for BUILD AND TESTS PASSED. It now
   builds the backtester, live scorer and both C++ test executables.
3. Double-click **RUN_LIVE.cmd**. It opens http://127.0.0.1:8768/ in your browser.

Keep the terminal open. Stop with Ctrl+C. If the browser does not open, enter that
address manually while the program is running. Do not double-click web/live.html:
it needs the local collector running behind it.

Python was already installed for your earlier project. This feature uses only
Python's standard library. C++ performs the regression, statistical screening and
current pair scoring; Python collects data, serves the UI and runs the optional
shadow ledger.

## Choose a mode

| Launcher | Behavior |
|---|---|
| RUN_LIVE.cmd | Current public prices, model diagnostics and provisional signals |
| RUN_LIVE_PAPER.cmd | Same feed plus local, simulated intraday entries and exits |
| RUN_LIVE_DEMO.cmd | Invented moving prices to see the interface and paper logic immediately |
| OPEN_REPORT.cmd | Original frozen historical backtest |
| OPEN_ALTERNATIVE.cmd | Historical results of one experimental alternative |

Run one monitor at a time on the default port. The demo is visibly labeled as
invented data. It does not require internet. The live modes require internet but
never request broker credentials, read an account, or send an order.

## What the screen tells you

- **Fresh pairs:** both last-trade timestamps meet the freshness rules during the
  provider's current regular session.
- **Current candidates:** fresh pairs whose training model passed and whose
  absolute z-score falls between the entry and stop thresholds.
- **Live Z:** distance from a fitted historical relationship. It is not a dollar
  fair-value estimate or probability of profit.
- **Screen rejected:** the daily historical relationship did not pass the fixed
  statistical checks. Prices may still be current.
- **Not current:** stale quotes, a closed session, unsynchronized timestamps or
  an error. Old values remain visible for inspection but cannot be candidates.

The first capture can take a minute or more. The default pause is 60 seconds
AFTER a capture completes; the browser checks for updates every two seconds.
Only two provider requests run concurrently. Failures trigger a longer pause.
This is polling, not tick-by-tick streaming.

The stock market is normally closed on weekends. Closed-session prices are
shown with timestamps and cannot create paper entries. In the archived live
connectivity test all four pairs were correctly blocked outside the session.

## Data and timing rules

The collector requests daily history from roughly the last three years and the
provider's latest regular-session trade. It excludes the quote's entire calendar
session from training, including when the quote is a previous day's last trade.
It never fills missing completed closes or silently aligns different calendars.

History and dividends are in the provider's current split-adjusted share units.
Forward total-return indices use (close + dividend) / prior close on that common
basis. The current session's cash distribution is included in the provisional
signal. Unlike the offline raw-share backtester, this signal-only calculation
does not separately multiply shares for splits; doing both would double-adjust.

The last 504 completed sessions fit the default model. This live study recalculates
the latest daily model when captures arrive. It is NOT the original frozen
63-session walk-forward execution schedule. Intraday deviations are provisional;
the original backtest used completed closing signals and later opening fills.
Do not transfer its backtest performance to this live or paper mode.

Freshness rules: quote age at most 120 seconds, cross-leg skew at most 30 seconds,
no quote more than five seconds ahead of the local clock, regular session open,
and recent completed training data. Keep your computer's clock correct. A public
feed may be delayed or unavailable; the monitor blocks rather than relabels an
old quote as fresh. It does not fall back to the old 2010–2023 dataset.

## Optional paper simulation

Paper mode is an intraday shadow experiment, not a broker paper account:

- Each pair has an independent, equal share of simulated starting capital.
- Two advancing candidate observations confirm direction; a third qualifying
  observation opens a hypothetical position. BOTH quote timestamps must advance.
- Fills are assumed at indicative last-trade prices plus configured adverse
  slippage and per-leg commissions. No bid/ask or available-depth claim is made.
- Short proceeds are liabilities, and borrow/financing accrue on elapsed calendar
  seconds using prior accepted marks. Intraday only: no dividend cash payments.
- Exit on convergence, a spread/capital stop, four elapsed hours, or a fresh
  observation in the last five minutes of the session. After an exit, that pair
  cannot reenter during the same session.
- No entry in the final 15 minutes. Daily model eligibility and freshness are
  required for entry. Repeated identical snapshots cannot generate fills.
- Stale/closed/error data pauses decisions. It does not invent an exit.
- If a position remains open into another date, it is marked unresolved and
  suspended. Overnight corporate actions and gap execution are NOT simulated.
- Closing the application does not liquidate a position. Every launch starts a
  new independent paper experiment; old journals remain available for inspection.

An exit reacts to an observed mark and assumes a fill at that mark plus costs;
it is not proof that an actual order could execute there. Borrow availability,
liquidity, trading halts and partial fills are not modeled.

## Alternative rules

The original default is retained. One explicit alternative uses 252 training
sessions, 21-session historical windows and entry |Z| >= 2.5. Its positive
historical result rests on only seven trades and is development evidence, not
independent validation. Read docs/STRATEGY_COMPARISON.md.

To inspect its live signals without replacing the default:

```powershell
py tools\live_monitor.py --config config\alternative.json
```

Add `--paper` for the alternative shadow study. Like the default live mode, it
refits on recent daily history rather than reproducing frozen backtest windows.
Configuration changes during a session stop new calculations; restart with a
new session to preserve an auditable set of assumptions.

## Saved files and troubleshooting

Each launch creates runs/live_TIMESTAMP with a frozen config, latest input,
latest scored result, latest display state, signals.jsonl and, in paper mode,
paper.json plus paper_events.jsonl. The latest full history is overwritten each
capture; journals append throughout the run. Each journal rotates after approximately
20 MiB and retains one previous segment. Archive sessions if you need older
observations beyond those two segments. Refreshing the browser
resets its short chart history but does not reset the running paper ledger.

If a provider error appears, keep the archived backtest separate from the live
monitor; do not interpret it as live data. Network restrictions, rate limits or
provider interface changes can interrupt this public endpoint. No account signup
is required to try the included adapter; a licensed feed would be a separate
integration with its own entitlements.

If the port is occupied, stop the other monitor or use:

```powershell
py tools\live_monitor.py --port 8769
```

Linux/macOS build:

```sh
g++ -std=c++17 -O2 src/live.cpp -o pairlab_live
python3 tools/live_monitor.py --exe ./pairlab_live
```
