# Product boundary for 0.1

The release succeeds when an ordinary workshop SQLite database can be browsed,
searched, filtered, and safely edited without Vulpes-specific code. Relationship
selection and an SQL console are included. The interaction quality of
`browse -> edit -> save` takes priority over expanding the feature list.

Explicitly out of scope: Lua, metadata tables, reports beyond reusable SQL-grid
results, form designers, plugins, networking, a server, GUI/web frontends, an
ORM, and a custom database or programming language.

The long-form originating plan is represented as actionable phases in
`TODO.md`; this document is the scope guard used during review.

