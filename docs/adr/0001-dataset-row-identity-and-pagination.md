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

The initial implementation uses bounded `LIMIT/OFFSET` pages. It provides a
deterministic default `ORDER BY` over the primary key when present. A requested
order is deterministic only if it is unique; keyset pagination therefore remains
the next implementation task and will append primary-key tie breakers where
needed. No UI API depends on offset mechanics.

## Consequences

- Widgets do not execute SQL or own statement lifetimes.
- Filtering is deliberately typed; free-form SQL belongs only in the SQL console.
- The model avoids injection through application/UI paths.
- Large datasets will improve without a widget API change when keyset paging lands.
- Updating data requires a separate editing model with explicit transactions.

