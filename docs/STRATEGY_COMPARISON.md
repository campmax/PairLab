# One alternative strategy experiment

The hypothesis: a shorter formation period and more frequent refits may respond to changing relationships; a higher entry threshold may avoid smaller deviations. Only this one alternative was added in this update. No parameter search was run.

| Rule | Original | Alternative |
|---|---:|---:|
| Training sessions | 504 | 252 |
| Historical trading window | 63 | 21 |
| Entry absolute Z | 2.0 | 2.5 |

Other settings, pairs, data and evaluation dates remain the same.

| Result | Original | Alternative |
|---|---:|---:|
| Total return | -1.917% | 1.554% |
| CAGR | -0.161% | 0.129% |
| Maximum drawdown | 2.253% | 0.399% |
| Completed trades | 15 | 7 |

## Alternative stress results

| Scenario | Return |
|---|---:|
| Double execution costs | 1.339% |
| 10% annual borrow | 1.419% |
| One extra session delay | 0.584% |
| Combined stress | 0.313% |

## Decision

The alternative remains experimental. Seven trades over almost twelve years and very little exposure do not establish reliable profitability. It was evaluated after inspecting the original results, on the same development history, so this is NOT an untouched validation result. The original baseline is retained rather than selecting a winner solely from these returns.

Open OPEN_ALTERNATIVE.cmd for the full interactive report. The live collector can use config/alternative.json explicitly. Intraday monitoring and shadow execution differ from this daily backtest; these returns do not validate that mode. No reserved-period performance was evaluated.
