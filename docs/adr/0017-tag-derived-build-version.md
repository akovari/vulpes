# ADR 0017: Derive build identity from release tags

- Status: accepted
- Date: 2026-08-28

## Context

The CMake project has a base semantic version, but development builds and
release artifacts also need an unambiguous source identity. Hard-coding a
second version in release scripts would let executable, archive, and tag names
drift apart.

Source archives may be configured without a Git checkout, and reproducible
packaging must be able to supply known metadata explicitly.

## Decision

Tags named `vMAJOR.MINOR.PATCH` are the release authority. CMake asks Git for a
long description and maps it to SemVer-compatible build identity:

```text
v1.2.3-0-gabcdef123456        -> 1.2.3
v1.2.3-4-gabcdef123456        -> 1.2.3-dev.4+gabcdef123456
v1.2.3-4-gabcdef123456-dirty  -> 1.2.3-dev.4+gabcdef123456.dirty
abcdef123456                  -> 0.1.0-dev+gabcdef123456
```

The generated `vulpes/version.hpp` exposes the display version, base project
version, commit, Git description, and dirty flag as typed constants. CMake
watches Git HEAD, its active reference, and index so ordinary rebuilds
reconfigure metadata after commits or staging changes.

When Git metadata is unavailable, the project version is the deterministic
fallback. Packagers may set `VULPES_BUILD_VERSION_OVERRIDE` for source archives;
the value is restricted to version-safe characters.

## Consequences

- `vulpes --version` identifies development binaries without timestamps.
- Exact release tags produce clean release versions with no development suffix.
- Packaging and runtime identity share one generated header and one tested
  formatting policy.
- An unstaged working-tree edit may require an explicit CMake reconfigure before
  the dirty suffix changes; Git index and commit changes are watched directly.
