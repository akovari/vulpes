# ADR 0007: Browse query controls

- Status: accepted
- Date: 2026-08-26

## Context

The first browse workflow needs interactive search, filters, sorting, and
refresh, but a terminal prompt must not turn into a channel for raw SQL or
unvalidated identifiers.

## Decision

The TUI uses `ui::TextPrompt`, a terminal-independent editing overlay, for
search and filter entry. `F3` searches schema-classified text fields. `F4`
filters the selected grid column using a deliberately small grammar: an optional
comparison operator (`=`, `!=`, `<>`, `<`, `<=`, `>`, `>=`) followed by a typed
value; `NULL` represents a null value. Blank input clears the applicable
constraint. `F6` toggles the selected field between ascending and descending
ordering, and `F5` refreshes.

The frontend resolves the selected field from `Grid` and invokes only typed
`Dataset` operations. The dataset validates fields, quotes identifiers, and
binds values, so no prompt content is assembled into SQL.

## Consequences

- Search, filters, sort, and refresh work consistently with all future
  renderers that use `Dataset`.
- The prompt widget can also serve a command line or other short text entry.
- Rich boolean/date filtering, saved filters, compound filters, and a full SQL
  console remain separate features with their own syntax and UI design.
