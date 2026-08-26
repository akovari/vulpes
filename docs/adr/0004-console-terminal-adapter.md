# ADR 0004: Console terminal adapter

- Status: accepted
- Date: 2026-08-26

## Context

The first browse screen needs raw keyboard input, resize awareness, ANSI frame
presentation, and reliable restoration of terminal state on Windows, Linux, and
macOS. The selected virtual-screen architecture forbids platform APIs in UI or
application code.

## Decision

`terminal::ConsoleTerminal` is the sole native terminal adapter. It owns setup
and restoration, maps Windows Console events and common Unix ANSI sequences to
`InputEvent`, reads terminal dimensions, and presents semantic frame diffs as
ANSI. `BrowseController` consumes only normalized events and `Dataset` methods.

The adapter intentionally has no business logic, widget behavior, SQL, or
localized strings. It is a replaceable implementation of `Terminal`.

## Current limitations

- Unix arrow/home/end/page navigation is normalized; non-ASCII text input is not
  decoded yet because browse does not accept text entry.
- Resize is checked before every frame. Signal-driven redraw and more complete
  terminal capability negotiation are pending.
- Real host testing is manual; deterministic behavior is covered through
  `TestTerminal`, frame-diff, and browse-controller tests.

