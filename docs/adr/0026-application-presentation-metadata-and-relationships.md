# ADR 0026: Application presentation metadata and relationship windows

- Status: accepted
- Date: 2026-08-28

## Context

SQLite schema facts are sufficient for a safe generated form, but not for a
polished application. Applications need labels, field order and visibility,
read-only presentation, explicit formats, relationship display/search fields,
and date/time meaning. These choices must remain independent of terminal
coordinates and must not weaken SQLite-derived write constraints.

## Decision

`appmeta::ApplicationMetadata` is the UI-neutral, validated semantic model.
`TableMetadata` and `FieldMetadata` enhance inspected `TableSchema` values. They
do not replace fields, foreign keys, generated-column flags, row identity, or
database capabilities. Metadata can make a field hidden or read-only, but cannot
make an intrinsically unsafe field writable.

Generated forms and grids consume resolved copies or stable references to this
model. Labels, ordering, visibility, currency/date presentation, and lookup
configuration therefore apply without introducing metadata-table or terminal
dependencies into widgets. A future SQLite metadata loader will populate the
same model.

`Dataset::lookup_options` accepts a validated display field, search fields,
search text, and bound limit. It quotes only inspected identifiers, binds search
values, and returns owned key/label pairs. `Dataset::related_record` returns an
owned schema/row pair. `RelationshipLookup` and `RelatedRecordView` call those
model operations and compose on the semantic `WindowStack`: Enter chooses a
relationship, F2 opens the selected related row, and Escape removes only the top
window.

Date/time meaning is opt-in metadata and is valid only for TEXT-affinity SQLite
columns:

- `date`: `YYYY-MM-DD`, with no time zone;
- `time`: `HH:MM` or `HH:MM:SS`, with no date or time zone;
- `date_time`: RFC 3339 `YYYY-MM-DDTHH:MM:SSZ` or an explicit numeric offset.

Date-time edits normalize to UTC `...Z` before persistence. Presentation uses an
explicit metadata IANA time zone. Invalid stored values remain visible in grids
with a warning marker; invalid edits stay in the form and map a validation error
to the annotated field.

## Consequences

- Ordinary SQLite databases retain the original inferred behavior.
- The same application definition can drive a future GUI or web renderer.
- Lookup search and drill-down remain bounded, synchronous local-database
  operations for now; no background threading contract is implied.
- SQLite-resident metadata schema, migration, and loading remain separate from
  the semantic model and can evolve without changing widget contracts.
