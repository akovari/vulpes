# ADR 0016: Small semantic layout primitives

- Status: accepted
- Date: 2026-08-28

## Context

Vulpes screens need repeatable measurement and layout behavior without adopting
a browser DOM or exposing a terminal framework's widget model. Hand-calculated
coordinates are already duplicated across dialogs and host messages, and will
become brittle as translations change label widths.

## Decision

Introduce a deliberately small semantic layout contract:

- `Constraints` clamps a requested logical-cell `Extent`.
- `Widget` measures, receives logical `Rect` bounds, and renders cells.
- `Label` measures Unicode display width and aligns text inside assigned bounds.
- `Container` arranges non-owning children in a horizontal or vertical flow,
  with spacing and optional growth weights.

Containers never own controls or application state. Their child references are
explicitly non-owning and require children to outlive the container. Layout
operates only in logical cells and knows nothing about ANSI, CPP-Terminal,
SQLite, actions, or localized message keys.

The undersized-terminal warning is the first production composition. It uses a
growing vertical container and a vertically centered label, replacing its
screen-specific row calculation.

## Consequences

- Screens can compose measured controls without becoming dependent on a large
  TUI framework.
- Translation width affects measurement rather than semantic state.
- Existing specialized widgets can migrate incrementally; they are not forced
  into a new hierarchy until layout behavior is useful to them.
- More sophisticated grids, overlays, wrapping, and responsive breakpoints
  remain specialized policies built on logical bounds.
