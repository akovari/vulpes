# ADR 0025: ICU message and display formatting

## Status

Accepted.

## Context

Simple `{name}` replacement cannot implement CLDR plural categories, translated
select branches, or locale-aware numbers and dates. Using `std::locale` would
also make behavior depend on host-installed locale data and would not provide a
portable message grammar. Some workspace messages are bound during presentation
construction and formatted later, so storing an unresolved string and replacing
braces inside a widget would bypass any real localization engine.

## Decision

Vulpes pins ICU4C through vcpkg and keeps every ICU type inside `src/core`.
`MessageValue`, `MessageArguments`, `LocalizedMessage`, and `LocaleFormatter`
remain Vulpes-owned interfaces.

Catalog values use ICU MessageFormat syntax with named, typed arguments.
`Localizer::bind` returns an owning message value containing its resolved pattern
and formatting locale, which permits delayed formatting without retaining a
catalog or localizer reference. Exact and language-only catalog matches format
with the requested regional locale; an English fallback formats with English
plural grammar.

`LocaleFormatter` uses ICU/CLDR for number, currency, date, time, and date-time
display. Currency formatting requires an explicit uppercase ISO 4217 code.
Date/time formatting requires an explicit IANA time-zone identifier. These are
display operations only: they do not parse or alter SQLite values.

Ordinary integer and real grid cells use locale-aware number display. Currency
and date/time formats are applied only after application metadata explicitly
annotates a field; declared SQLite type names alone are insufficient evidence.

## Consequences

- Czech can use `one`, `few`, and `other` branches rather than English-style
  suffixes, and translated select branches remain in one catalog message.
- Catalog patterns are parsed and validated when loaded; format failures become
  structured metadata errors rather than uncaught ICU status codes.
- ICU adds binaries and locale data to packages. Release packaging must collect
  its runtime libraries and licenses.
- ASCII apostrophes have MessageFormat quoting semantics. Catalog prose should
  use typographic apostrophes or language-appropriate quotation marks.
- Locale-aware editing/parsing and SQLite date inference remain separate future
  policies; formatted display text is never written back implicitly.
