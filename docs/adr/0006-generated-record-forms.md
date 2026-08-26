# ADR 0006: Generated record forms

- Status: accepted
- Date: 2026-08-26

## Context

Vulpes must make an ordinary SQLite table editable without hand-written form
code. The first implementation must preserve the database and dataset
boundaries while providing a useful keyboard workflow.

## Decision

`ui::RecordForm` is a terminal-independent semantic widget backed by a
`model::Dataset` draft. It starts either insert or edit mode, renders to a
`terminal::ScreenBuffer`, and sends validated typed values back through the
dataset on save. `Esc` cancels the draft; `F8` saves it. A database or
validation error is rendered in the form while retaining the draft.

Controls are inferred conservatively from `FieldSchema`: numeric declared types
use numeric fields, explicit boolean declarations and a narrow set of common
names use checkboxes, and generated, existing primary-key, and BLOB fields are
read-only. SQLite schema names are the initial labels. The form owns transient
text editing state but not persistence or SQL generation.

## Consequences

- The TUI and future frontends share the same editing lifecycle and validation.
- A BLOB cannot accidentally be replaced with the displayed placeholder text.
- Labels, ordering, lookup controls, dialogs, and metadata overrides remain
  deliberate follow-up work rather than implicit terminal layout policy.
- When an error message names a schema field, the form selects and marks it;
  ambiguous errors map only when one editable field changed.
