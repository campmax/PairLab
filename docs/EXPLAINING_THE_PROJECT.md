# Explain PairLab in plain language

## The 30-second version

“I built a C++ research tool to test pairs trading. It uses earlier stock data to
estimate how two related stocks tend to move together, then trades only a later
period. I included the costs of holding short positions and found that the fixed
baseline lost money after costs. The project is about checking a hypothesis
carefully, not claiming to have discovered a profitable trading strategy.”

## What is a pair?

Two stocks studied together. For instance, two payment companies might respond to
some similar business conditions. That does not mean they have identical risks or
must return to their old relationship.

## What does shorting mean?

Borrow shares, sell them, and later buy shares back to return them. You benefit
if the price falls. If the price rises, buying them back costs more. You also pay
borrow costs and owe dividends on shares you are short. Short-sale cash is not
free spending money or an immediate gain.

## How does the strategy decide?

1. Study the previous 504 sessions, about two trading years.
2. Estimate the relationship and test whether its unexplained difference appears
   stable enough for this particular screening rule.
3. If the pair fails, hold cash for its next window.
4. If it passes and its difference becomes unusually large, take opposite sides
   of the two stocks at a later open.
5. Close after convergence, a time limit, a risk stop or the scheduled boundary.
6. Repeat with a new training window. Earlier profits do not choose the next pair.

## What is the hedge ratio?

How strongly stock A's log total-return path is related to stock B's path in the
training regression. It also controls the relative dollar sizing of the legs.
If beta is 1.5, B receives 1.5 times A's absolute dollar exposure. Opposite signs
reduce some shared exposure but do not make the portfolio risk-free or necessarily
market-neutral.

## What is a z-score?

The difference from the fitted relationship measured in units of its training
variation. Z = 2 means two training standard deviations above the fitted line.
It is not a 95% prediction guarantee; financial observations are dependent and
may have heavy tails or changing relationships.

## Why aren't correlated stocks enough?

Correlation concerns co-movement. Two prices can rise together while the gap
between them drifts without returning. The residual screen asks a different
question about the estimated relationship. Its statistical assumptions can still
fail, and historical mean reversion can disappear.

## Why execute the next day?

You only know the completed closing price after it occurs. Using it to decide
and also pretending to buy at that exact close would give the simulation an
unavailable advantage. The program creates the signal after the close and uses
a later open with adverse execution costs.

## What did the first experiment find?

The fixed rules produced 15 trades over almost 12 years, with a −1.92% portfolio
return and 2.25% maximum drawdown. Only 12 of 192 pair-windows were eligible.
Most capital sat idle. A small drawdown therefore does not mean the strategy has
excellent risk-adjusted performance; cash exposure explains much of it.

The limited number of trades makes strong statistical conclusions inappropriate.
The useful conclusion is that these particular rules did not demonstrate an edge
on this selected historical dataset. It does not prove that all pairs trading
is unprofitable, or that loosening a filter would work.

## Questions you should be able to answer

- Which observations were available before a given trade?
- Why did this pair fail a formation window?
- How do splits and short dividends affect cash and holdings?
- Why are short-sale proceeds excluded from profit?
- Where do borrow rates and slippage assumptions come from?
- Why does testing more pairs or settings create false-discovery risk?
- What does the project omit about real trading?
- What would count as a fair test of your next idea?

## Reading the source

Start at Ledger in src/engine.hpp: follow a buy and a short sale with a calculator.
Then trace one order from a closing z-score to its next-open fill. Read the data
loader and only then the statistical routines. The tests show small cases where
the expected answer is known. You do not need to memorize every implementation
detail to explain the economic and statistical assumptions accurately.
