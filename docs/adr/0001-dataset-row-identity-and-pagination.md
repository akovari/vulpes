# ADR 0001: Dataset row identity and pagination

- Status: accepted
- Date: 2026-08-26

## Context

Grids and forms need stable logical rows without retaining SQLite statements.
SQLite tables need not have a primary key, views might not be editable, and an
unbounded `OFFSET` scan becomes expensive on large datasets.

## Decision

`db::Row` is an owning snapshot of the current statement row. `model::Dataset`
is table-backed and owns its page, current-row position, typed filters, search,
and ordering. Only schema-discovered table/column identifiers are quoted into
queries; all values are parameter-bound.

A table is editable only when it is not a view and has an explicit primary key.
The dataset returns a `RowIdentity` containing every primary-key field, including
composite keys. Tables without primary keys and views remain browseable but must
advertise no edit capability.

The dataset uses keyset paging when a single stable key is available: its default
single-column primary-key order, or an explicitly selected non-null unique key.
The next and previous page predicates compare against the last or first visible
key, so inserts before the current page cannot create duplicate rows. Composite,
nullable, and non-unique sorts use the existing bounded `LIMIT/OFFSET` fallback
until their null and tie-break semantics are deliberately designed. No UI API
depends on the query strategy.

## Consequences

- Widgets do not execute SQL or own statement lifetimes.
- Filtering is deliberately typed; free-form SQL belongs only in the SQL console.
- The model avoids injection through application/UI paths.
- Large stable-key datasets avoid expensive OFFSET scans without a widget API
  change.
- A keyset refresh deliberately returns to the first matching page because an
  in-memory page anchor is not a durable bookmark across writes.
- Updating data requires a separate editing model with explicit transactions.
