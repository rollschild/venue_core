# `venue_core` specification

## 1. Basics

- Instrument: one, so no symbol field. Multiple instruments are excluded from the scope.
- Prices: integer ticks; `[min_price, max_price]`; outside it → reject with `invalid_price`
- Quantities: valid range `1` to `max_order_quantity` (at most `2^32`)
- Order IDs: uniqueness rule set in configs;
  - `session_unique`: ID never reused in this session, even after its order has died
  - `strictly_increasing`: each new ID must be greater than last accepted one (OUCH rule)
- Sides: `buy` and `sell`; Time in force: GTC (good til cancel), IOC (immediate or cancel)
- Command sequence: set by upstream; strictly increasing from one command to the next
- Venue sequence: starts at `1` and rises by `1` for each event, across both channels

## 2. Commands

Every command also carries `command_sequence`.

- `Cancel`:
  - Fields: `order_id`
  - Meaning: Remove what is left of a live order
- `SubmitLimit`:
  - Fields: `order_id`, `side`, `limit_price`, `quantity`, `tif`
  - Meaning: Match first, then GTC rests the remainder and IOC cancels it
- `Reduce`:
  - Fields: `order_id`, `new_total_quantity`
  - Meaning: A new total, not an amount to subtract. Sending it twice gives same result. Keeps the order's place in the queue (OUCH quote 2)
- `Replace`:
  - Fields: `old_order_id`, `new_order_id`, `new_price`, `new_total_quantity`, `tif`
  - Meaning: A new ID and a new place in the queue. The quantity already filled carries over
- `Uncross` (operator only):
  - Fields: `reference_price`
  - Meaning: Operator only: ends the opening auction

## 3. Validation (checks run in this order; the first failure wins)

- A reject changes no book state.
- A rejected command still advances `last_command_sequence`: `last_command_sequence` advances on every command whose sequence is in order, whether it's accepted or rejected. The one exception is a command rejected for an out-of-order sequence, which doesn't move it.
- Reject events: SubmitLimit → `OrderRejected`; Cancel and Reduce → `CancelRejected`;
  Replace → `ReplaceRejected`; Uncross → `ControlRejected`. Each one carries the reason.

| Command     | #   | Rule (the command fails if …)                                         | Reject reason                       |
| ----------- | --- | --------------------------------------------------------------------- | ----------------------------------- |
| SubmitLimit | 1   | `command_sequence` ≤ the last command sequence                        | `non_monotonic_command_sequence`    |
| SubmitLimit | 2   | `order_id` was already used this session (policy `session_unique`)    | `duplicate_order_id`                |
| SubmitLimit | 2   | `order_id` ≤ the last accepted ID (policy `strictly_increasing`)      | `non_increasing_order_id`           |
| SubmitLimit | 3   | `limit_price` outside `[min_price, max_price]`                        | `invalid_price`                     |
| SubmitLimit | 4   | `quantity` = 0 or > `max_order_quantity`                              | `invalid_quantity`                  |
| SubmitLimit | 5   | `tif` = IOC and the venue is in the pre-open auction                  | `ioc_in_auction`                    |
| Cancel      | 1   | `command_sequence` ≤ the last command sequence                        | `non_monotonic_command_sequence`    |
| Cancel      | 2   | `order_id` is unknown, or the order is already filled or canceled     | `unknown_or_terminal_order_id`      |
| Reduce      | 1   | `command_sequence` ≤ the last command sequence                        | `non_monotonic_command_sequence`    |
| Reduce      | 2   | `order_id` is unknown, or the order is already filled or canceled     | `unknown_or_terminal_order_id`      |
| Reduce      | 3   | `new_total_quantity` < filled, or ≥ the current total                 | `invalid_reduction`                 |
| Replace     | 1   | `command_sequence` ≤ the last command sequence                        | `non_monotonic_command_sequence`    |
| Replace     | 2   | `old_order_id` is unknown, or the order is already filled or canceled | `unknown_or_terminal_order_id`      |
| Replace     | 3   | `new_order_id` was already used this session (`session_unique`)       | `duplicate_replacement_id`          |
| Replace     | 3   | `new_order_id` ≤ the last accepted ID (`strictly_increasing`)         | `non_increasing_order_id`           |
| Replace     | 4   | `new_price` outside `[min_price, max_price]`                          | `invalid_price`                     |
| Replace     | 5   | `new_total_quantity` = 0 or > `max_order_quantity`                    | `invalid_quantity`                  |
| Replace     | 6   | `new_total_quantity` ≤ the quantity already filled                    | `replacement_below_filled_quantity` |
| Replace     | 7   | `tif` = IOC and the venue is in the pre-open auction                  | `ioc_in_auction`                    |
| Uncross     | 1   | `command_sequence` ≤ the last command sequence                        | `non_monotonic_command_sequence`    |
| Uncross     | 2   | the venue is not in the pre-open auction                              | `not_in_auction`                    |

## 4. Events

- Each event carries: `venue_sequence` (the next number), `channel` (private or public), and its payload.
- One command makes zero or more events, with consecutive venue sequences. The order of events
  inside one command is part of the contract (defined per scenario in §6, Ph2).
- Terms: `match_id` = one ID per fill, shared by every event about that fill. `leaves` = the
  quantity still open after the event. `maker` = the resting order in a fill; `taker` = the incoming order.
- The public channel reports changes to the book only. An order that never rests never appears (D2).
- A full execution implies removal: a public `OrderExecuted` that takes leaves to 0 removes the
  order, and no `OrderDeleted` follows it (blueprint K7).

### Private channel (to the order's owner)

| Event             | Fields                                                                                       | When                                       |
| ----------------- | -------------------------------------------------------------------------------------------- | ------------------------------------------ |
| `OrderAccepted`   | `order_id`, `side`, `price`, `quantity`, `tif`                                               | A submit passed validation                 |
| `OrderRejected`   | `command_sequence`, `order_id`, `reason`                                                     | A submit failed validation                 |
| `CancelRejected`  | `command_sequence`, `order_id`, `reason`                                                     | A cancel or a reduce failed validation     |
| `ReplaceRejected` | `command_sequence`, `old_order_id`, `new_order_id`, `reason`                                 | A replace failed validation                |
| `ControlRejected` | `command_sequence`, `reason`                                                                 | An uncross failed validation               |
| `OrderExecuted`   | `order_id`, `match_id`, `price`, `quantity`, `leaves`, `liquidity` (maker, taker or auction) | One per order per fill                     |
| `OrderCanceled`   | `order_id`, `canceled_quantity`, `reason` (user or IOC remainder)                            | Open quantity removed by the venue or user |
| `OrderReduced`    | `order_id`, `new_total_quantity`, `leaves`, `removed_quantity`                               | A reduce was applied                       |
| `OrderReplaced`   | `old_order_id`, `new_order_id`, `price`, `new_total_quantity`, `leaves`                      | A replace was applied                      |

### Public channel (market data, no owner)

| Event           | Fields                                              | When                                              |
| --------------- | --------------------------------------------------- | ------------------------------------------------- |
| `AddOrder`      | `order_id`, `side`, `price`, `quantity`             | An order now rests in the book                    |
| `OrderExecuted` | `order_id`, `match_id`, `quantity`                  | A resting order traded (at its own price)         |
| `OrderCanceled` | `order_id`, `canceled_quantity`                     | Part of a resting order was removed               |
| `OrderDeleted`  | `order_id`                                          | All of a resting order was removed                |
| `OrderReplaced` | `old_order_id`, `new_order_id`, `price`, `quantity` | A resting order was replaced                      |
| `Trade`         | `match_id`, `price`, `quantity`, `aggressor` side   | A print of a fill. OPEN: exact use decided in Ph2 |
| `Cross`         | `price`, `quantity`                                 | The auction result (Ph8)                          |

## 5. Decisions on edge cases

| Case                                                    | Outcome                                                                                             | Why                                  |
| ------------------------------------------------------- | --------------------------------------------------------------------------------------------------- | ------------------------------------ |
| Quantity 0                                              | Reject `invalid_quantity`                                                                           | §3 Submit #4, Replace #5             |
| Price outside `[min_price, max_price]`                  | Reject `invalid_price`                                                                              | §3 Submit #3, Replace #4             |
| Submit with an ID already used                          | Reject `duplicate_order_id` (`session_unique`) or `non_increasing_order_id` (`strictly_increasing`) | §3 Submit #2                         |
| Submit that reuses the ID of a filled or canceled order | Rejected as a duplicate: IDs are never reused in a session                                          | §1 Order IDs                         |
| Cancel, reduce or replace of an unknown or dead order   | Reject `unknown_or_terminal_order_id`                                                               | §3 #2                                |
| Reduce up (new total > current total)                   | Reject `invalid_reduction`. To increase, use replace (it loses priority)                            | §3 Reduce #3                         |
| Reduce to the current total                             | Reject `invalid_reduction` (no change)                                                              | §3 Reduce #3                         |
| Reduce to exactly the filled quantity                   | Accepted: leaves = 0, the order is done and leaves the book                                         | §3 Reduce #3 lets filled ≤ new total |
| Reduce below the filled quantity                        | Reject `invalid_reduction`                                                                          | §3 Reduce #3                         |
| Replace with identical price and quantity               | Accepted: new ID, back of the queue                                                                 | D3                                   |
| Replace to a total ≤ the filled quantity                | Reject `replacement_below_filled_quantity`                                                          | §3 Replace #6                        |
| IOC before the open                                     | Reject `ioc_in_auction`                                                                             | §3 Submit #5, Replace #7             |
| Uncross during continuous trading                       | Reject `not_in_auction`                                                                             | §3 Uncross #2                        |
| Two faults in one command                               | The reason of the first check that fails, in §3 order (bad price + quantity 0 → `invalid_price`)    | §3 header                            |
| Rejected command with an in-order sequence              | `last_command_sequence` advances                                                                    | D1                                   |
| Resend of a command already processed                   | Reject `non_monotonic_command_sequence`; nothing else changes                                       | D1, §3 #1                            |
| Taker filled completely on arrival                      | No public event for the taker; only executions of the orders it hit                                 | D2, §4                               |

## 6. Later (leave empty today)

Matching rules and event sequences (Ph2–Ph3) · reduce and replace (Ph4) · auction (Ph8) · snapshot (Ph10)
