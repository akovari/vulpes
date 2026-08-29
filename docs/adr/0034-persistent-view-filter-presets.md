# ADR 0034: Persistent typed filter presets for application views

- Status: accepted
- Date: 2026-08-29

## Context

The Dataset already represents a reusable filter as ordered, typed comparison
terms and optional search text. Keeping that state only in a document loses a
useful part of a portable application view between launches. Persisting a raw
`WHERE` clause instead would couple metadata to SQLite text parsing, discard
typed values, and let an application definition bypass Dataset validation.

## Decision

Metadata schema version 5 adds `_app_view_filters` and
`_app_view_filter_terms`. A filter is owned by one `_app_views` row and has a
stable name, position, and optional text search. Its ordered terms reference an
inspected field, one closed `FilterOperator` value, and an exactly typed SQLite
value represented as `null`, `integer`, `real`, `text`, or `blob`. The storage
schema checks the type/value-column relationship; the loader repeats that
validation, parses no SQL, and validates fields and NULL comparison semantics
against the view's inspected SQLite schema.

`_app_views.default_filter_name` optionally names one of the view's filters.
When a frontend opens the view it copies all of that view's presets into its
Dataset and applies the default once. Dataset remains the sole owner of active
query state and SQL generation. Widgets, grids, and the metadata loader do not
form a `WHERE` clause.

## Consequences

- Named filters are portable with the `.vulpes` database and survive a restart.
- Filter values retain their SQLite type, including NULL and blobs.
- Invalid names, unknown fields, invalid comparisons, mismatched type columns,
  duplicate presets, and missing defaults fail definition loading before UI
  rendering.
- The future view designer edits these semantic records through a validated
  application model. It does not expose raw SQL or terminal state.
