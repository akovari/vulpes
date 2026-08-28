# ADR 0022: Explicit localized menu mnemonics

- Status: accepted
- Date: 2026-08-28

## Context

Vulpes initially treated the first code point of every translated menu label as
its mnemonic. This caused collisions such as multiple “Open” items, prevented a
translator from choosing a more natural later character, and made activation
ambiguous. Rendering and input each repeated the inference independently.

## Decision

Every menu-bar and pop-up-menu label has an adjacent catalog key ending in
`.mnemonic`. Its value contains exactly one Unicode code point. During
`WorkspaceText` construction Vulpes verifies that:

- the mnemonic occurs in the translated label under Unicode lowercase matching;
- no two assignments in the same static menu scope collide;
- label and mnemonic arrays have identical sizes.

`Workspace` uses the validated code point for both keyboard comparison and the
underlined display cell. Mnemonics may occur anywhere in a label. Invalid
catalog metadata fails before opening the workspace.

## Consequences

- Alt and item shortcuts are deterministic in English, Czech, and future
  locales.
- Translators own mnemonic choice and must translate mnemonic keys together with
  menu labels.
- Dynamic menus will require equivalent validation when their item model is
  introduced.
