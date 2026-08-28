# ADR 0015: Guard interactive terminal startup

- Status: accepted
- Date: 2026-08-28

## Context

The workspace, browse view, SQL console, and input diagnostics require both an
interactive input stream and an interactive output stream. Entering raw mode
when either stream is redirected can hang a pipeline, emit control sequences
into a file, or leave the host terminal partially configured after an
initialization failure.

A full-screen failure dialog cannot safely be rendered when output itself is
not a terminal. Capability reporting must therefore also work without starting
the terminal backend.

## Decision

`terminal::detect_console_capabilities` checks standard input and standard
output at the host boundary. `ConsoleTerminal` rejects an unusable combination
before constructing CPP-Terminal's session or changing raw-mode options. It
wraps initialization, input, size, and output failures as structured terminal
errors; its owned initializer continues to restore host state through RAII.

`vulpes --terminal-capabilities` reports the detected stream states without
starting the TUI. A rejected interactive startup writes a plain diagnostic to
standard error and emits no ANSI control sequences. Non-interactive commands,
including schema listing and version output, remain redirectable.

## Consequences

- Shell pipelines cannot accidentally start a full-screen Vulpes session.
- Capability diagnostics remain useful when either standard stream is
  redirected.
- An invalid output stream receives a plain error rather than a screen-buffer
  dialog, because rendering such a dialog would reproduce the failure.
- Platform-host verification is still required for consoles that report as
  interactive and for raw-mode restoration after real input events.
