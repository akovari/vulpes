# ADR 0019: Shared UTF-8 single-line editor

- Status: accepted
- Date: 2026-08-28

## Context

Path, command, search, filter, and generated-form fields originally appended
printable keys to a string and removed only its final code point. Left, Right,
Home, End, and forward Delete did nothing. Besides feeling unlike a desktop RAD
tool, duplicating this behavior across prompts and forms made fixes diverge.

The terminal backend does not expose a portable host cursor and application
widgets must remain independent of CPP-Terminal.

## Decision

Add `ui::LineEditor`, a semantic single-line control that stores UTF-8 text and
a byte-offset cursor. Every operation preserves a code-point boundary:

- Left/Right move by one encoded code point;
- Home/End move to the logical endpoints;
- Backspace and Delete erase the preceding or following complete code point;
- printable character input inserts at the cursor;
- a display-cell viewport keeps the cursor visible in a bounded field;
- rendering marks a cell as the logical caret by deriving from the supplied
  semantic input style.

The editor has no database, theme, terminal-backend, or application-command
dependency. Callers supply a logical rectangle and style. `TextPrompt` and
generated text/number fields compose it. Checkboxes and relationship lookups
retain their own semantic key behavior.

## Consequences

- All ordinary single-line entry supports familiar in-place editing and shares
  deterministic UTF-8 tests.
- The stored value never contains viewport truncation or caret markers.
- The cursor is code-point safe, but full grapheme-cluster navigation remains
  governed by the Unicode policy in ADR 0002.
- Selection, word movement, clipboard integration, bracketed paste, and
  overwrite mode remain explicit tasks in `TODO.md`; multiline SQL editing is
  addressed separately by ADR 0020.
