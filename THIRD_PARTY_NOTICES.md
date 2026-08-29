# Third-party notices

Vulpes is MIT licensed; see [LICENSE](LICENSE). Release archives also contain
this notice. The release process must preserve the license texts distributed by
all bundled dependencies.

| Component | Use in Vulpes | License / source |
| --- | --- | --- |
| SQLite | Database engine | Public domain, [sqlite.org](https://www.sqlite.org/copyright.html) |
| ICU4C | Localization and locale formatting | Unicode License, [unicode-org/icu](https://github.com/unicode-org/icu) |
| utf8proc | UTF-8 and terminal-cell handling | MIT, [JuliaStrings/utf8proc](https://github.com/JuliaStrings/utf8proc) |
| CLI11 | Process command-line parsing | BSD-3-Clause, [CLIUtils/CLI11](https://github.com/CLIUtils/CLI11) |
| nlohmann/json | JSON catalogs and metadata | MIT, [nlohmann/json](https://github.com/nlohmann/json) |
| dacap/clip | Clipboard adapter | MIT, [dacap/clip](https://github.com/dacap/clip) |
| CPP-Terminal | Terminal host adapter | MIT, [jupyter-xeus/cpp-terminal](https://github.com/jupyter-xeus/cpp-terminal) |
| zlib | PDF stream compression | zlib License, [madler/zlib](https://github.com/madler/zlib) |
| Lua | Sandboxed business-logic runtime | MIT, [lua.org](https://www.lua.org/license.html) |
| PDFio | PDF export implementation | Apache-2.0, [michaelrsweet/pdfio](https://github.com/michaelrsweet/pdfio) |
| Roboto Regular | Embedded Unicode PDF font | SIL Open Font License 1.1, Copyright 2011 The Roboto Project Authors, [googlefonts/roboto-classic](https://github.com/googlefonts/roboto-classic) |

Catch2 is used for tests only and is not included in runtime archives. CMake
installs the resolved license texts for the listed vcpkg dependencies plus
CPP-Terminal, PDFio, and Roboto under `share/doc/Vulpes/licenses`; PDFio and
Roboto are fetched at fixed revisions recorded in `CMakeLists.txt`. Review the
resolved dependency set before public distribution, especially if a manifest or
toolchain changes.
