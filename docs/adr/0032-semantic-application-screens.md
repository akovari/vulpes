# ADR 0032: Semantic application screens and dashboards

- Status: accepted
- Date: 2026-08-29

## Context

Application mode can already open named forms, views, reports, commands, and
menus, but it lacks a first-class application landing surface. Reusing the
workspace home directly would make application navigation dependent on TUI
layout. Storing terminal coordinates would make the same definition unusable by
a future GUI or web frontend.

## Decision

Metadata schema version 4 introduces `_app_screens` and `_app_screen_items`.
`ScreenDefinition` owns a stable name, label, optional description, an optional
single default role, and ordered items. Each item owns a label, optional
description, and references a named `_app_commands` entry.

`ApplicationDefinition::validate` rejects duplicate screen names, empty labels
or present-but-empty descriptions, more than one default screen, more than
1,000 items, and unknown item command references. No screen table accepts
coordinates, colors, terminal control codes, raw SQL, or frontend-specific
widget identifiers.

The command boundary adds `screens` and `screen <name>`. The runtime resolves
them into semantic `CommandResponse` values. Application launch opens the
default screen when present. `ScreenDocument` uses the existing `WindowFrame`,
`Button`, normalized input, and theme roles to render an ordered action list.
On Enter it returns only the selected command name; the workspace re-parses and
executes that command through `ApplicationRuntime`, just as it does for an
application menu item.

## Consequences

- Application landing dashboards work without custom code or TUI metadata.
- Screen navigation cannot bypass named-command validation, Lua command hooks,
  recursion limits, or report read-only boundaries.
- The TUI obtains a basic dashboard now while future frontends remain free to
  choose their own layout and controls.
- Form, menu, and screen designers can extend the semantic model later without
  preserving terminal coordinates as application data.
