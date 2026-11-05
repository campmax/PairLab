# Diagnostic extension

The new Diagnostics tab summarizes existing audit records. No strategy parameters, data or numerical backtest results were changed.

## Training filters

| Pair | Eligible | Residual failures | Unstable hedge | Half-life failures |
|---|---:|---:|---:|---:|
| KO / PEP | 0 / 48 | 48 | 25 | 6 |
| XOM / CVX | 2 / 48 | 46 | 18 | 11 |
| V / MA | 8 / 48 | 40 | 9 | 0 |
| JPM / BAC | 2 / 48 | 46 | 10 | 15 |

Failures overlap; their counts must not be added together.

## Exit reasons

| Reason | Trades | Net P&L |
|---|---:|---:|
| spread_stop | 3 | $-1,481.17 |
| mean_reversion | 1 | $348.89 |
| window_end | 5 | $-495.73 |
| time_stop | 5 | $-446.52 |
| scheduled_end | 1 | $157.24 |

## Cost attribution

Net P&L: $-1,917.29. Recorded execution and holding costs: $737.89. Adding those costs back gives $-1,179.40. Net cash dividends remain included.

This is an accounting attribution for the recorded trades, not a rerun with zero costs. A cost change could alter position sizing and exits. Costs alone do not explain the loss. Small-sample outcome statistics are descriptive, not forecasts.
