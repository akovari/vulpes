# Product boundary for 0.1

The release succeeds when an ordinary workshop SQLite database can be browsed,
searched, filtered, and safely edited without Vulpes-specific code. Relationship
selection and an SQL console are included. The interaction quality of
`browse -> edit -> save` takes priority over expanding the feature list.

The runtime starts in a keyboard-driven workspace, whether or not a database
path is supplied. Its initial tabbed/pane shell provides application menus,
portable open/create path entry, modal input, status feedback, and documents
for browse and SQL work. This is intentionally a classic local RAD workflow,
not a browser-style application.

The implemented post-0.1 foundation also includes optional SQLite-resident
application metadata, named read-only Grid reports, and application menus. Lua
hooks remain a later layer. Form designers, plugins, networking, a server,
GUI/web frontends, an ORM, and a custom database or programming language remain
out of scope.

The long-form originating plan is represented as actionable phases in
`TODO.md`; this document is the scope guard used during review.
