# Research design and accounting

## Fixed question and universe

The question is whether a relationship between two related stocks can support
net mean-reversion returns in subsequent windows. The four pairs were selected
for business similarity before this project's first run, not ranked using the
reported P&L: KO/PEP, XOM/CVX, V/MA and JPM/BAC. This is still a retrospective
survivor universe, not a historically assembled opportunity set.

Each pair starts with $25,000 in a separate sleeve. Capital is not reassigned
from a rejected pair to another pair. Sleeve profits compound within that sleeve;
capital is not equalized again at boundaries. There is at most one position per
pair and no shared symbols across pairs.

## Data and corporate actions

The importer requests daily Yahoo chart data using explicit period endpoints.
`range=max` is not accepted as a guarantee of daily granularity. Dates and daily
granularity are checked; currencies must be USD. All symbols must have exactly
the same calendar as SPY, with no forward filling or silent row dropping.

Yahoo OHLC is split-adjusted. Multiply each earlier price and dividend by the
product of reported splits AFTER that date to reconstruct then-current share
units. A split on the date itself is excluded from that multiplier because the
session trades in new units. Apply the explicit split to held shares before
booking that date's dividend and before open execution. Both long and short
quantities are adjusted. This relies on complete provider action records and is
not proof that every corporate event has been captured correctly.

The total-return index is constructed forward:

```
TRI[t] / TRI[t-1] = split[t] * (close[t] + dividend[t]) / close[t-1]
```

This assumes a mathematical reinvestment at the closing price for SIGNALS only.
The account credits/debits dividends as cash, with no automatic reinvestment.
Ex-date accrual approximates payment timing. No dividend-adjusted execution price
is used, so cash distributions are not deliberately counted twice.

## Formation and screening

At each boundary i, use observations [i−504, i), exclusively before the trading
window. Fit `log(TRI_A) = alpha + beta*log(TRI_B) + residual` by QR ordinary least
squares. Estimate the residual sample standard deviation with n−1 denominator.

Test the residual with:

```
Δe[t] = gamma*e[t−1] + theta*Δe[t−1] + error[t]
```

The baseline has one augmentation lag, no second-stage constant, and a constant
in the first-stage regression. Its statistic is gamma divided by its OLS standard
error. Use MacKinnon's two-series constant-specification 1% critical value:
`−3.89644 − 10.9519/(n−1) − 33.527/(n−1)^2`. Do not use ordinary univariate ADF
critical values on fitted cointegrating residuals. These coefficients are from
statsmodels' MacKinnon tables; its BSD license is included.

Additional filters:

- beta in [0.2, 5]; do not flip regression direction to get a favorable result.
- AR(1) residual half-life `−ln(2)/ln(phi)` in [1, 60] sessions, with 0 < phi < 1.
- Absolute difference of first-half and second-half betas divided by full-window
  |beta| must be at most 0.5 (denominator floor 0.01).
- Residual standard deviation must be at least 0.005 log units.

The method assumes suitable I(1) inputs but does not enforce a separate
integration-order test. A unit-root rejection under inappropriate assumptions
is not reliable evidence of cointegration. The four fixed per-window tests at
1% give a Bonferroni upper bound of 4% on any false rejection in that window
under their assumptions; repeated windows and later researcher changes are not
covered by this bound. No p-values or calibrated probability-of-profit claims
are displayed.

## Signal and execution

Freeze alpha, beta and residual scale throughout the 63-session trading window.
After each close calculate `z = (log(TRI_A) − alpha − beta*log(TRI_B))/sigma`.
Enter when 2 ≤ |z| < 4 and the screen passed. Low z buys A and shorts B; high z
shorts A and buys B. Allocate gross notional in a 1:beta ratio, with 100% total
gross relative to sleeve equity. This is neither guaranteed dollar neutrality
nor market-beta neutrality.

Determine share quantities from the signal close, then execute at a later open
with adverse 5bp slippage and $1 per leg per fill. Pending quantities adjust for
intervening splits. The model assumes fractional shares and simultaneous fills
on both legs. At that open, reject the whole entry if the opening gross exceeds
110% of its configured target relative to equity or fails the modeled capital
check. This is a simplified opening-price risk check, not an exchange market-on-
open auction simulation; no partial fills or daily-volume cap are modeled.

Normal exits are convergence past the 0.5 threshold in the position's direction
or 20 held sessions. Risk exits are |z| ≥ 4, a loss of 10% of entry sleeve equity,
or equity ≤ 30% of gross exposure. Risk exits execute at the next open and block
new entries for that window. Other orders follow the configured delay. An
already queued normal exit is replaced only by a risk exit. Exit thresholds are
not intraday stop orders and cannot cap gap losses.

Close old positions at the next boundary's scheduled open, cancel pending orders,
then fit the new model. New signals begin at that boundary day's close. The final
backtest session is a scheduled open liquidation. This calendar is fixed before
simulation; no final closing-price signal receives an impossible same-close fill.

## Cash, equity and holding costs

`equity = cash + shares_A*price_A + shares_B*price_B` with signed quantities.
A short sale credits cash and adds a negative market liability. It is not profit.

Borrow charge = prior-close short market value × annual borrow rate × actual
calendar days/365. Baseline borrow is 3%; the explicit stress uses 10%. Financing
charge = max(0, prior-close short liability − cash) × annual financing rate ×
calendar days/365; rate 8%. This treats current short liability as restricted
collateral. It is an approximation, not a broker statement model. Cash yield,
short rebates and risk-free return are zero. No taxes or exchange fee schedule.

Per trade, net P&L equals cash after exit minus equity immediately before entry.
It therefore includes commissions, slippage, dividends owed/received, borrow,
financing and all price movements. The program checks that completed-trade P&L
reconciles with total account change after final liquidation. It stops with an
error if a sleeve becomes insolvent rather than producing misleading returns.

## Comparisons and metrics

SPY and an equal-weight initial stock basket enter at the first evaluation open
and liquidate at the final evaluation open. They use the same slippage and per-
fill commission assumptions, cash dividends and split handling. The basket is
not rebalanced. These are opportunity-cost comparisons with different exposures,
not equal-risk benchmarks. Cash is flat at 0% by assumption.

Return includes all costs. CAGR uses elapsed calendar days from the close before
the first evaluation session through the final session. Volatility and Sharpe use
sample daily-return standard deviation and sqrt(252); Sharpe is null for a flat
series. Maximum drawdown includes initial capital and uses daily closes. It does
not measure worst intraday loss. Fraction invested means at least one open pair
at the daily close, not capital-weighted exposure.

Stress scenarios are fixed: double slippage and commissions; 10% borrow; one
extra session of normal-order delay; and their combination. Risk exits still
use the next open. These are diagnostics, not parameter optimization.

## References

- https://www.statsmodels.org/stable/generated/statsmodels.tsa.stattools.coint.html
- https://www.statsmodels.org/stable/generated/statsmodels.tsa.stattools.adfuller.html
- MacKinnon, J. G. (2010), Critical Values for Cointegration Tests.
- https://ranaroussi.github.io/yfinance/ (provider-data conventions and research context;
  the shipped importer uses the public chart endpoint directly, not yfinance).
