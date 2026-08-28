# ADR 0021: Grid position, overflow, and column sizing

- Status: accepted
- Date: 2026-08-28

## Context

The initial Grid could navigate datasets larger than the screen, but it always
rendered the beginning of a dataset page. Once selection moved below the visible
body, the highlight disappeared. Users also had no indication of absolute row
or column position, hidden rows or columns, empty results, or control over widths.

These are presentation concerns. They must not add terminal coordinates to the
dataset or make SQL-result Grids behave differently from browse Grids.

## Decision

Grid owns a viewport within its current dataset page or owned result rows. It
keeps selection visible and renders:

- localized absolute row and selected-column position in the footer;
- left/right border arrows when columns exist outside the visible range;
- a proportional right-border track and selection thumb when rows overflow;
- a localized empty-state message when the row source is empty;
- per-field width overrides adjusted by semantic narrow/widen actions, bound to
  Ctrl+Left and Ctrl+Right by default.

`Dataset::total_count` caches the filtered count until the next refresh. This
avoids issuing a count query for every frame while ensuring query, edit, and
explicit refresh operations invalidate the value.

## Consequences

- Browse and SQL results share the same navigation affordances and sizing model.
- Widths are document-local presentation state; persistent saved views remain a
  metadata feature.
- The scrollbar represents the selected absolute row rather than accepting
  mouse input. Mouse interaction remains optional and deferred.
