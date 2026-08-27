# ADR 0014: CPP-Terminal backend adapter

- Status: accepted
- Date: 2026-08-27

## Context

The first native `ConsoleTerminal` implementation proved the Vulpes terminal
boundary, but it also required Vulpes to maintain raw-mode setup, input event
decoding, resize handling, and restoration separately for Windows and POSIX.
That work is platform-sensitive and distracts from Vulpes' database and RAD
experience.

Vulpes must retain its own semantic input events, virtual screen, frame diff,
and window manager. Replacing those layers with a full TUI framework would
couple the application model to one renderer and undermine future frontends.

## Decision

Adopt the pinned CPP-Terminal source as the host-terminal implementation behind
the existing `vulpes::terminal::Terminal` interface. The adapter translates
CPP-Terminal keyboard and resize events to Vulpes `InputEvent` values and uses
the existing frame-diff/ANSI encoder for rendering.

CPP-Terminal is not exposed from public Vulpes headers. `ScreenBuffer`, UI
widgets, actions, and application code continue to depend only on Vulpes
abstractions. The dependency is pinned to a commit in CMake; upgrades require
an explicit review and platform verification.

## Consequences

- Raw terminal lifecycle and host-specific event transport are delegated to a
  maintained cross-platform library.
- The current `ConsoleTerminal` class remains the Vulpes adapter name, keeping
  application call sites stable while its implementation changes.
- Deterministic Vulpes rendering and fake-terminal tests remain intact.
- Manual platform-specific console code can be removed once the adapter passes
  the documented Windows, Linux, and macOS verification matrix.
