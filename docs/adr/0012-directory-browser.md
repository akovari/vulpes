# ADR 0012: Keep path entry and directory browsing as separate file-open paths

## Status

Accepted.

## Context

Path entry is portable, scriptable, and essential for network paths or a file
the user already knows. It is less convenient for interactive discovery. A
directory browser improves the no-argument, keyboard-first workspace without
turning file selection into a platform-specific native-dialog dependency.

## Decision

Vulpes provides a semantic `DirectoryBrowser` alongside the existing path
prompt. It lists the current directory with directories first, supports parent
navigation, Home/End, type-to-select, and keyboard selection. It never creates,
renames, or deletes filesystem entries. Selecting a regular file merely returns
its path to the workspace; SQLite remains responsible for validating the file.

The browser is invoked from File and Database menus. Manual path entry remains
available for both read/write and explicit read-only opening, and Create remains
the only operation that uses SQLite create mode.

## Consequences

- File browsing is deterministic and testable through `ScreenBuffer` and
  normalized events.
- No terminal widget depends on Windows, macOS, or Linux native dialogs.
- A selected file is not assumed to be a database; a structured database error
  is shown if SQLite cannot open it.
- Future native dialogs may be optional host adapters, not the application
  model.
