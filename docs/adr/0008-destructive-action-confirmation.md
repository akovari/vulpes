# ADR 0008: Destructive action confirmation

- Status: accepted
- Date: 2026-08-26

## Context

The initial Vulpes workflow supports database record deletion. Keyboard-driven
applications must make the destructive path deliberate without embedding
database logic in a terminal widget.

## Decision

The browse frontend opens `ui::ConfirmationDialog` for `Delete`. The dialog
defaults to Cancel; only an explicit selection followed by Enter confirms the
action. It receives title, body, labels, and instructions from `Localizer` and
renders only to `ScreenBuffer`. When confirmed, the frontend calls
`Dataset::erase()`.

## Consequences

- An accidental Delete then Enter cancels rather than removes a record.
- The existing dataset transaction, primary-key identity, and concurrency check
  remain the only write implementation.
- Referential-integrity errors and field-level recovery will gain a dedicated
  structured error dialog in a subsequent UI slice.
