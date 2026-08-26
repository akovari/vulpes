# ADR 0013: Tabbed workspace shell

- Status: accepted
- Date: 2026-08-26

## Context

Vulpes must feel like a practical FoxPro/Pascal/NC-style database environment,
not an executable that only works when launched with a database argument. It
needs menus, a command-oriented home screen, file opening, modal input, and
multiple work surfaces while remaining usable in SSH and small terminals.

## Decision

Vulpes will start into a keyboard-first workspace when no database is supplied.
The initial chrome has `File`, `Database`, `View`, `Window`, and `Help` menus,
a status bar, and an active-document area. The first file interaction is a
portable path-entry dialog for open/create; it does not depend on a native file
picker.

The workspace uses tabbed/pane documents rather than overlapping MDI windows.
Browse, SQL console, forms, and dialogs become documents or modal surfaces. A
future GUI may choose native windows while consuming the same semantic actions
and document model.

## Consequences

- `vulpes` and `vulpes database.db` share one application shell.
- F10 and semantic menu actions work reliably across terminal hosts; Alt-key
  shortcuts are optional enhancements rather than required interaction.
- Tab management is deterministic and testable in `ScreenBuffer` tests.
- Native directory pickers, drag-and-drop, and overlapping MDI windows are
  explicitly deferred.
