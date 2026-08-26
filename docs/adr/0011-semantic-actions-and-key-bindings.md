# ADR 0011: Semantic actions and key bindings

- Status: accepted
- Date: 2026-08-26

## Context

Terminal hosts describe the same user intent with different physical events.
Vulpes also needs configurable bindings, localized help, and future frontends
without teaching application controllers about ANSI sequences or Windows virtual
keys.

## Decision

`core::ActionMap` converts normalized `terminal::KeyEvent` values to stable
`ActionId` values. The default map contains the browse keys documented by the
product, while `ActionMap::bind` allows a caller to replace a mapping. Browse
controllers receive `ActionId` and operate datasets through the model boundary.

Action IDs are stable machine identifiers, not displayed strings. The initial
implementation keeps mappings in memory. A user configuration file is deferred
until the configuration subsystem has a versioned format and migration policy.

## Consequences

- Adding or changing a terminal backend does not change browse-controller
  behavior.
- Tests can verify actions without emulating a specific host terminal.
- Future TUI, GUI, and web frontends can map their own gestures to the same
  application intent.
- Keybinding persistence and conflict reporting remain explicit follow-up work.
