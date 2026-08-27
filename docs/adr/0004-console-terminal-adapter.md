# ADR 0004: Console terminal adapter

- Status: accepted
- Date: 2026-08-26

## Context

The first browse screen needs raw keyboard input, resize awareness, ANSI frame
presentation, and reliable restoration of terminal state on Windows, Linux, and
macOS. The selected virtual-screen architecture forbids platform APIs in UI or
application code.

## Decision

`terminal::ConsoleTerminal` is the sole Vulpes terminal adapter. It owns a
CPP-Terminal session for setup, restoration, native keyboard events, and resize
events, then maps those events to `InputEvent`. It reads terminal dimensions and
presents semantic frame diffs as ANSI. `BrowseController` consumes only
normalized events and `Dataset` methods.

The adapter intentionally has no business logic, widget behavior, SQL, or
localized strings. It is a replaceable implementation of `Terminal`.

## Current limitations

- CPP-Terminal covers the common key, modifier, Unicode, and resize transport;
  real host verification is still required before each supported-platform claim.
- Vulpes intentionally ignores mouse and focus events until corresponding
  semantic UI behavior is implemented.
- Deterministic behavior is covered through `TestTerminal`, frame-diff,
  browse-controller, workspace, and adapter normalization tests.

## Consequences

- Unix input decodes UTF-8 into one normalized character event. Ctrl+A through
  Ctrl+Z normalize to their printable base character with the `ctrl` modifier,
  matching Windows Console behavior for semantic actions such as Ctrl+C.
