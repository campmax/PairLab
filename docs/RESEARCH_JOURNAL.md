# Research journal

## Baseline — 2026-10-03

Before the first real-data strategy run, fixed four disjoint pairs, 504 formation
sessions, 63 trading sessions, one residual-test augmentation lag, a 1% screen,
entry/exit/stop at 2/0.5/4, 20-session time stop and the settings in default.json.
Selected survivors by business similarity. No parameter optimizer or profitable-
pair ranking was used. This is retrospective development, not formal preregistration.

Data request using yfinance was rate-limited. The importer was changed to a public
chart request. A max-history response was observed to return quarterly data;
explicit start/end timestamps plus a daily-granularity check resolved that issue.
The final importer fetched and validated 3,522 daily sessions for each symbol.

First real-data result: −1.9173% total return, 2.2532% maximum drawdown, 15 trades.
Subsequent changes addressed source formatting, reporting, tests and instructions;
no trading parameter was changed to improve these results.

Interpretation: these rules did not demonstrate an edge. Low drawdown largely
reflects infrequent exposure. The small number of trades is a limitation, not a
reason to lower a statistical threshold until a pleasing result appears.

## Next experiments — not yet run

- Diagnose why KO/PEP fails the predefined screens and why XOM/CVX's eligible
  windows produce no entries. Treat this as explanation, not automatic retuning.
- Seek a point-in-time stock universe including delisted names and historical
  borrow data before making broader performance claims.
- Specify one alternative rule and its rationale before viewing its results.
  For example, test a longer formation period because relationship estimates
  may be noisy. Keep both successful and unsuccessful results in this journal.
- Validate an unchanged design on a reserved later period only after deciding
  the rules. Record the first access; later reuse becomes further development.

For each future experiment record: hypothesis, exact config, data fingerprints,
which periods have already been inspected, result, and what would falsify the idea.
