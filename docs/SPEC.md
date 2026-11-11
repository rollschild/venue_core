# `venue_core` specification

## 1. Basics

- Instrument: one, so no symbol field. Multiple instruments are excluded from the scope.
- Prices: integer ticks; `[min_price, max_price]`; outside it → reject with `invalid_price`
- Quantities: valid range `1` to `max_order_quantity` (at most `2^32`)
- Order IDs: uniqueness rule set in configs;
  - `session_unique`: ID never reused in this session, even after its order has died
  - `strictly_increasing`: each new ID must be greater than last accepted one (OUCH rule)
- Sides: `bid` and `ask`; Time in force: GTC (good til cancel), IOC (immediate or cancel)
- Command sequence: set by upstream; strictly increasing from one command to the next
- Venue sequence: starts at `1` and rises by `1` for each event, across both channels

## 2. Commands

- **Cancel**: remove a resting order. Fields: `command_sequence`, `order_id`.
- **SubmitLimit**: … Fields: …
- **Reduce**: … Fields: …
- **Replace**: … Fields: …
- **Uncross** (operator only): … Fields: …

## 3. Validation (checks run in this order; the first failure wins)

- General: a reject changes no book state. Does a reject advance the command sequence? …
  | Command     | #   | Rule                          | Reject reason   |
  | ----------- | --- | ----------------------------- | --------------- |
  | SubmitLimit | ?   | price outside the valid range | `invalid_price` |
  | …           |     |                               |                 |

## 4. Events

- Every event gets the next venue sequence. …

### Private channel (to the order's owner)

- **OrderRejected**: a submit failed validation. Fields: `command_sequence`, `order_id`, `reason`.
- …

### Public channel (market data, no owner)

- …

## 5. Decisions on edge cases

- Quantity 0: …
- (one line for each edge case in my last message)

## 6. Later (leave empty today)

Matching rules and event sequences (Ph2–Ph3) · reduce and replace (Ph4) · auction (Ph8) · snapshot (Ph10)
