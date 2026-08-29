# ADR 0033: Grid projections, summaries, and filter presets

- Status: accepted
- Date: 2026-08-29

## Context

The browse Grid needs the conveniences expected in a practical database
application: identifying columns that remain visible during horizontal
navigation, read-only calculated values, summaries, and reusable filters. None
of these should make a widget construct SQL, make metadata migration a
requirement for an ordinary SQLite database, or couple a future frontend to
terminal coordinates.

## Decision

`GridOptions` describes a semantic presentation projection:

- `frozen_columns` pins the leading visible columns while the remaining columns
  scroll horizontally;
- `GridCalculatedColumn` has a stable non-SQL identifier, a presentation
  label, and a function over an already-owned `db::Row`; calculated columns are
  explicitly read-only and never resolve to a writable `FieldSchema`; and
- `GridAggregation` supplies a localized presentation label together with a
  typed `model::AggregateDefinition`.

The Grid remains a renderer. For a table-backed Grid it asks the `Dataset` for
aggregate values; `Dataset` validates every inspected field, selects SQL
aggregate functions only from an enum, quotes identifiers, and binds the
existing typed filter/search values. For owned SQL/report rows, Grid evaluates
the same aggregate definitions locally without a database dependency.

`Dataset` also owns named `SavedFilter` presets. A preset is an in-memory copy
of the current typed filters and optional search text. Applying one validates
its inspected fields and refreshes the Dataset once. `BrowseDocument` exposes
the same semantic operations to any frontend. Version 5 application metadata
can supply portable view-owned presets, as specified by ADR 0034; the frontend
copies them into the Dataset at document creation rather than giving a Dataset
access to application tables.

## Consequences

- Frozen columns, calculated cells, and summaries use the same API in terminal,
  GUI, or web frontends.
- Widgets neither concatenate prompt text into SQL nor retain a raw SQLite
  handle.
- A calculated column cannot be sorted as a database field or edited through a
  generated form.
- Aggregate results describe the active Dataset query, including its typed
  filters and text search. Aggregate labels are provided by the caller so they
  can be localized at the presentation boundary.
- Aggregate summaries are presentation-only and are not appended to the
  existing row-oriented CSV, JSON, text, HTML, or PDF exports. A future report
  layout owns any exported summary placement or formatting.
- Ad-hoc presets survive refreshes within an open browse document. Portable
  view-owned presets additionally survive process restarts in a `.vulpes`
  application.
