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

## Consequences

- Generated forms give readable relationship selection for small local tables.
- A hard limit of 100 avoids unintentionally loading a large related table.
- Search, async paging, composite foreign keys, display overrides, and
  related-record drill-down need deliberate metadata and interaction design.
