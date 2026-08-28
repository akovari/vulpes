# ADR 0018: Semantic TUI visual system

- Status: accepted
- Date: 2026-08-28

## Context

The workspace initially had theme-aware menu and status rows, but application
documents and controls still used default terminal colors and independently
drawn ASCII borders. Grids assigned every visible field the same width, pop-up
menus hid their shortcuts, compact path prompts could be positioned outside the
screen, and large generated forms could move focus to an invisible field.

These are shared rendering problems. Fixing them in individual screens would
duplicate policy and make a future GUI renderer harder to separate from the
terminal frontend.

## Decision

Extend the semantic `ThemeRole` vocabulary to cover desktop and document text,
muted and error messages, borders and shadows, inputs, disabled controls, and
the distinct regions of a grid. Pass the selected immutable `Theme` into hosted
documents and controls, including direct document sessions.

Render logical window chrome through `WindowFrame` using Unicode box-drawing
cells. Frames clear their content rectangle and may draw a clipped one-cell drop
shadow. They still know nothing about ANSI sequences, console handles, input,
or application state.

Use measured pop-up-menu widths, right-aligned shortcut labels, separator rows,
disabled-item styles, and first-character item mnemonics. `Workspace` retains
the menu's semantic selection and enablement rules. `WindowManager` measures tab
labels in display cells and keeps the active tab in the visible subset.

Let `Grid` calculate bounded preferred widths from field headings and values in
its already-owned page. Minimum widths and the existing horizontal viewport
remain deterministic. Give the selected row and focused cell separate theme
roles. Let generated forms derive a small field viewport from focus and render
overflow indicators when all fields do not fit.

## Consequences

- The default and high-contrast palettes now cover the full ordinary workflow,
  not only workspace chrome.
- Widget tests can assert semantic roles and logical Unicode cells without
  screenshots or a terminal backend.
- Pop-up restoration remains a normal previous-versus-current frame diff; no
  widget emits ad hoc erase sequences.
- Unicode box drawing is part of the TUI presentation contract. The existing
  Unicode-capable terminal requirement and diagnostics remain the host boundary.
- Cursor-aware editing, explicit locale mnemonic metadata, adjustable columns,
  and a general nested window stack remain separate follow-up work recorded in
  `TODO.md`.
