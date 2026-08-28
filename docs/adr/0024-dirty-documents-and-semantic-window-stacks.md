# ADR 0024: Dirty documents and semantic window stacks

## Status

Accepted.

## Context

The first workspace shell represented each transient surface with an unrelated
`optional` and kept only one modal title in `WindowManager`. That was adequate
for a path prompt, but it could not express a record form opening a relationship
lookup and then a related-record drill-down. Tabs also gave no indication that
an SQL script or form draft contained unsaved work.

## Decision

Documents expose a read-only `is_dirty()` semantic property. Generated forms
are dirty after a field changes; SQL documents are dirty when their nonblank
source differs from the last successfully executed source. The workspace copies
that state to its document descriptor, and `WindowManager` appends `*` to dirty
tab titles without changing the stable document ID.

`WindowManager` stores workspace modal titles as a stack. Escape dismisses only
the top title. `WindowStack<Content>` is a reusable owning stack for document
overlays. Its content type belongs to the hosting surface and may be a variant;
the stack itself knows only stable descriptors, layer roles, dirty state, and
last-opened-first-routed ordering.

Browse now owns forms, prompts, and confirmations as variant layers in a
`WindowStack`. It renders bottom-to-top and sends input only to the top layer.

## Consequences

- Nested relationship windows can use the same routing rule without adding
  another top-level optional or terminal-specific coordinate state.
- Dirty markers remain presentation metadata and never alter document identity.
- Closing a dirty document still uses the existing deliberate close
  confirmation; a later save/discard policy can inspect the same dirty state.
- Overlay content remains semantic and testable without a terminal backend.
