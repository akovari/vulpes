# ADR 0005: Dataset edit lifecycle

- Status: accepted
- Date: 2026-08-26

## Context

The browse grid and future generated forms need one shared editing API. Letting
either frontend formulate `INSERT`, `UPDATE`, or `DELETE` statements would leak
SQLite details into UI code and make validation, transaction handling, and error
recovery inconsistent.

## Decision

`model::Dataset` has three explicit modes: `browse`, `insert`, and `edit`.
`begin_insert()` and `begin_edit()` create an in-memory draft. `set()` accepts
only schema-discovered writable fields; generated and hidden fields are never
writable, and primary-key fields cannot change during an edit. A table is
editable only when it is not a view and has an explicit primary key.

`save()` validates the draft, writes with parameter binding inside a
`db::Transaction`, and refreshes the page only after commit. Updates and deletes
use the primary-key values captured when editing began, including composite keys.
If a SQLite constraint or validation error occurs, the draft remains intact for
the caller to correct or cancel. Inserted fields that were never set are omitted
from the `INSERT`, allowing SQLite defaults to apply.

## Consequences

- Forms, keyboard actions, and a future GUI share the same edit semantics.
- The 0.1 model deliberately disallows primary-key edits; supporting them later
  requires explicit identity and relationship-update policy.
- Optimistic concurrency beyond a changed/deleted-row check is deferred until
  there is a user-visible conflict-resolution design.
