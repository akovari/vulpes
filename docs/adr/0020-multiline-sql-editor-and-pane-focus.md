# ADR 0020: Multiline SQL editor and pane focus

- Status: accepted
- Date: 2026-08-28

## Context

The first SQL console could append text, add a newline, and erase only the final
code point. It could not move a cursor or keep source visible once it exceeded
the window. After execution, ordinary arrow actions were always diverted to the
result Grid, leaving no way to revise source with the keyboard.

The SQL console must remain independent of database execution, terminal escape
sequences, and the result renderer. Single-line fields already have a focused
UTF-8 editor, but multiline movement additionally needs line boundaries, a
preferred display column, and persistent viewport state.

## Decision

Add `ui::MultilineEditor`, a terminal-independent UTF-8 source model with:

- code-point-safe Left/Right, Backspace, and forward Delete;
- logical Up/Down movement that preserves the desired display-cell column;
- line-local Home/End and viewport-relative PageUp/PageDown;
- Enter line splitting, edge deletion line joining, and four-column Tab stops;
- stable vertical and horizontal offsets that keep the logical caret visible;
- display-cell slices and clipping flags for deterministic rendering.

`SqlConsole` composes this model and renders a separate prompt gutter, editor
surface, logical caret, and border clipping indicators. It still never executes
SQL.

`SqlDocument` owns focus between the editor and an optional result Grid. The
semantic `document.switch_pane` action, bound to F7 by default, toggles focus.
Arrow actions edit source while the editor is focused and navigate results while
the Grid is focused. Escape from the result pane returns to the editor before a
subsequent Escape closes the document.

## Consequences

- Long SQL scripts can be revised without rebuilding them from the end.
- Wide characters affect visual columns correctly while byte offsets remain at
  UTF-8 code-point boundaries.
- Editor and result navigation no longer compete for the same arrow events.
- Grid can render a persistent but subdued unfocused selection; browse Grids
  retain focused behavior by default.
- Grapheme-cluster navigation, selection, clipboard integration, bracketed
  paste, undo/redo, syntax highlighting, completion, and history remain deferred.
