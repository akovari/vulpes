# ADR 0012: SQL console input and Grid result reuse

- Status: accepted
- Date: 2026-08-26

## Context

Vulpes needs an interactive SQL escape hatch, but it must not bypass the
database boundary or introduce a second table renderer. The console must accept
multiline scripts while retaining the TUI-independent architecture.

## Decision

`ui::SqlConsole` is a terminal-independent multiline editor. It returns an
explicit execute result for `F8` and never owns a database connection. The
frontend calls `Database::run_sql`, which already produces bounded, owned final
statement results. `ui::GridRows` adapts those owned columns and rows to the
existing `Grid` renderer.

The `sql` semantic command and `--sql` CLI flag open the console. Arrow keys
navigate a displayed result grid; `Enter` adds a source line and `Esc` exits.

## Consequences

- Query results have one renderer and one Unicode/layout behavior.
- The SQL editor, result adapter, and database execution can be tested without
  a host terminal.
- The first version intentionally lacks history, SQL completion, parameter
  prompts, explicit transaction controls, and multiple result-set display.
