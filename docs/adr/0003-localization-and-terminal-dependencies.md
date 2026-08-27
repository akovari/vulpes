# ADR 0003: Localization and terminal dependencies

- Status: accepted
- Date: 2026-08-26

## Context

Vulpes must support user-facing translations without translating database
identifiers, SQL, action IDs, or metadata keys. It must also remain portable
across Windows, Linux, and macOS without rebuilding the application engine for a
specific TUI toolkit.

## Decision

All user-facing text is addressed by stable message keys and rendered through
`core::Localizer`. Catalog fallback follows `language-region -> language -> en`.
Messages use named placeholders. Initial English catalog data is compiled in;
external catalog loading and ICU plural/select formatting are deferred until the
first non-English shipped catalog requires them.

CLI11 is used for process-level flags and help. Its API handles normal and wide
Windows argument vectors. The command-palette parser remains small application
code because it parses a persistent command language (`browse "table name"`), not
an `argv` vector; it produces stable semantic `CommandId` and action IDs.

The existing virtual-screen renderer remains the frontend boundary. FTXUI was
considered but would replace that boundary. CPP-Terminal is adopted as a pinned
source dependency for host input and terminal lifecycle because it is not
available from the pinned vcpkg registry. It is used only through a `Terminal`
adapter, never from application or widget code; see ADR 0014.

## Consequences

- Localized UI strings will not alter commands, metadata, or database behavior.
- Adding pluralization needs one localized formatting implementation, not a
  rewrite of every caller.
- CLI parsing does not grow a home-grown flags implementation.
- Terminal backend selection remains reversible.
