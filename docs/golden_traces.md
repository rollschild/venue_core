# Golden traces

Each trace is a list of commands and the full list of events that they must make.
Work out the events by hand from `SPEC.md` only.
Do not use the oracle or the engine to get the expected events.
The tests in `tests/` copy these lists. The oracle and every engine must give them exactly.

## Format

One block for each command. One line for each event, in order:

```
seq <n>  <command>
  #<venue sequence>  <PRV or PUB> <Event>{<fields>}
```

For `OrderExecuted`, give: order ID, `match_id`, price, quantity, leaves, liquidity.
For `Trade`, give: `match_id`, price, quantity, aggressor.

## Trace 1: a sweep at maker prices

It pins: price priority, time priority, fills at the maker price, the fill group,
leaves on both sides, removal at leaves 0 with no `OrderDeleted`, a partly filled maker
that stays, D2 (a taker that fills completely is not public), and a GTC remainder that rests.

Configuration: the default `VenueConfig` (continuous trading, `session_unique`).

### Commands

```
seq 1  SubmitLimit id 1  Sell 3 @ 101 GTC
seq 2  SubmitLimit id 2  Sell 2 @ 101 GTC
seq 3  SubmitLimit id 3  Sell 4 @ 103 GTC
seq 4  SubmitLimit id 4  Buy  8 @ 104 GTC
seq 5  SubmitLimit id 5  Buy  3 @ 103 GTC
```

### Method

Keep the book on paper next to the events. Update it after each fill.

For each command:

1. Write `PRV OrderAccepted` (SPEC §6.2 step 1).
2. Find the maker: the best price first, then the order that rested first (SPEC §6.1).
   Stop when no maker price is inside the taker's limit, or the taker leaves are 0.
3. For each fill: quantity = the smaller leaves; price = the maker's price;
   `match_id` = the next one. Update both leaves, then write the 4 events (SPEC §6.2 step 2).
4. A maker at leaves 0 leaves the book. Write no `OrderDeleted` for it (SPEC §4).
5. Taker leaves > 0 at the end: GTC writes `PUB AddOrder` with the leaves (SPEC §6.2 step 3).

### Expected events

Notes after `--` tell where each value comes from. They are not part of the events.

```
seq 1  SubmitLimit id 1  Sell 3 @ 101 GTC
  #1  PRV OrderAccepted{order 1, Sell, 101, qty 3, Gtc}
  #2  PUB AddOrder{order 1, Sell, 101, qty 3}
                                   -- no bid in the book, so no fill; GTC rests all 3

seq 2  SubmitLimit id 2  Sell 2 @ 101 GTC
  #3  PRV OrderAccepted{order 2, Sell, 101, qty 2, Gtc}
  #4  PUB AddOrder{order 2, Sell, 101, qty 2}
                                   -- same price as order 1, but later: behind it in time

seq 3  SubmitLimit id 3  Sell 4 @ 103 GTC
  #5  PRV OrderAccepted{order 3, Sell, 103, qty 4, Gtc}
  #6  PUB AddOrder{order 3, Sell, 103, qty 4}

  Book after seq 3:  asks  101: order 1 (3), then order 2 (2)
                           103: order 3 (4)
                     bids  none

seq 4  SubmitLimit id 4  Buy 8 @ 104 GTC
  #7  PRV OrderAccepted{order 4, Buy, 104, qty 8, Gtc}
                                   -- before all fills (SPEC §6.2 step 1)

  Fill 1: the lowest ask is 101 <= 104; at 101, order 1 rested first.
          qty = min(taker leaves 8, maker leaves 3) = 3; price = maker price 101; match 1.
          After: order 1 leaves 0; order 4 leaves 5.
  #8  PRV OrderExecuted{order 1, match 1, 101, qty 3, leaves 0, Maker}
  #9  PRV OrderExecuted{order 4, match 1, 101, qty 3, leaves 5, Taker}
                                   -- the taker gets 101, not its limit 104
  #10 PUB OrderExecuted{order 1, match 1, qty 3}
  #11 PUB Trade{match 1, 101, qty 3, aggressor Buy}
                                   -- order 1 is at leaves 0: it leaves the book, no OrderDeleted
                                   -- OrderDeleted means "removed for a reason other than a trade".

  Fill 2: the lowest ask is 101 <= 104; at 101, order 2.
          qty = min(taker leaves 5, maker leaves 2) = 2; price = maker price 101; match 2.
          After: order 2 leaves 0; order 4 leaves 3. -- #12 is venue sequence
  #12  PRV OrderExecuted{order 2, match 2, 101, qty 2, leaves 0, Maker}
  #13  PRV OrderExecuted{order 4, match 2, 101, qty 2, leaves 3, Taker}
  #14 PUB OrderExecuted{order 2, match 2, qty 2}
  #15 PUB Trade{match 2, 101, qty 2, aggressor Buy}

  Fill 3: the lowest ask is 103 <= 104; at 103, order 3.
          qty = min(taker leaves 3, maker leaves 4) = 3; price = maker price 103; match 3.
          After: order 3 leaves 1; order 4 leaves 0.
  #16  PRV OrderExecuted{order 3, match 3, 103, qty 3, leaves 1, Maker}
  #17  PRV OrderExecuted{order 4, match 3, 103, qty 3, leaves 0, Taker}
  #18 PUB OrderExecuted{order 3, match 3, qty 3}
  #19 PUB Trade{match 3, 103, qty 3, aggressor Buy}

  Book after seq 4:  asks  103: order 3 (1)
                     bids  none

seq 5  SubmitLimit id 5  Buy 3 @ 103 GTC
  #20 PRV OrderAccepted{order 5, Buy, 103, qty 3, Gtc}

  Fill 1: the lowest ask is 103 <= 103; at 103, order 3 (what is left after seq 4).
          qty = min(taker leaves 3, maker leaves 1) = 1; price = maker price 103; match 4.
          After: order 3 leaves 0; order 5 leaves 2.
  #21 PRV OrderExecuted{order 3, match 4, 103, qty 1, leaves 0, Maker}
  #22 PRV OrderExecuted{order 5, match 4, 103, qty 1, leaves 2, Taker}
  #23 PUB OrderExecuted{order 3, match 4, qty 1}
  #24 PUB Trade{match 4, 103, qty 1, aggressor Buy}
                                   -- order 3 is at leaves 0: it leaves the book, no OrderDeleted

  No ask is left: the fills stop with taker leaves 2.
  #25 PUB AddOrder{order 5, Buy, 103, qty 2}
                                   -- GTC remainder (SPEC §6.2 step 3): rests at its limit 103,
                                   -- quantity = leaves 2, not 3; no private event for it
```

### Book after seq 5

```
asks  none
bids  103: order 5 (2), filled 1 of 3
```

Totals: 25 events (#1 to #25), `match_id` 1 to 4. The book is not crossed.
