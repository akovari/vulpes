# ADR 0009: Generated relationship lookups

- Status: accepted
- Date: 2026-08-26

## Context

An ordinary foreign key such as `orders.customer_id` should be usable in a
generated form without forcing the user to remember an integer key. The first
implementation must preserve the rule that widgets do not query SQLite.

## Decision

`Dataset::lookup_options(field, limit)` verifies that the field has a discovered
foreign key, inspects the referenced schema, and returns a bounded list of
key/label pairs. It chooses a display column by exact name in this order:
`name`, `title`, `description`, `code`; otherwise it uses the referenced key.
All identifiers derive from inspected schema and the limit is bound as a
parameter.

`RecordForm` treats a foreign-key field as a lookup control. It renders the
related label and cycles the list with Left/Right, while persistence receives
the original referenced key.

The later searchable window remains bounded and accepts only inspected display
and search-field identifiers. `Dataset::lookup_option` resolves the form's
current key separately from that bounded page. This matters for existing rows:
the current relationship must retain its readable label even when it sorts
after the first page. F2 obtains an owned related row through
`Dataset::related_record`; neither relationship widget executes SQLite itself.

## Consequences

- Generated forms give readable, searchable relationship selection for local
  tables without leaking a raw key at page boundaries.
- A configurable hard limit, validated between 1 and 1,000, avoids
  unintentionally loading an entire related table.
- Composite foreign keys and async/keyset lookup paging still require deliberate
  model and interaction design.
