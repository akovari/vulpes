# ADR 0002: Unicode cells and terminal rendering

- Status: accepted
- Date: 2026-08-26

## Context

Terminal grids cannot measure text by UTF-8 byte length or Unicode code-point
count. A CJK character normally occupies two cells, while combining marks occupy
none. Terminal byte encodings and Windows console APIs must not leak into widgets.

## Decision

Vulpes uses utf8proc 2.11.3 for Unicode code-point width and UTF-8 decoding.
`ScreenBuffer` remains a logical cell model. Its UTF-8 writer reserves
continuation cells for wide code points; renderers skip those cells. Invalid UTF-8
at this boundary is a terminal error.

Widgets emit cells only. `diff_frames` computes deterministic semantic operations
(`move_cursor`, `set_style`, `write`); ANSI encoding translates those operations
separately. `TestTerminal` owns queued normalized events and captured frames for
unit and eventual integration tests.

Full extended grapheme-cluster layout is not yet implemented. The current writer
does not retain uncomposable combining sequences. Text editing must introduce
grapheme-aware storage and cursor logic before it is exposed to users.

## Consequences

- Grid rendering is deterministic and testable without a terminal host.
- ANSI, Windows Console, and future GUI renderers consume the same frame diff.
- Unicode handling has a named limitation that cannot silently become a text-input
bug.

