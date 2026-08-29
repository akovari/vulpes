# Building a release archive

Release archives are generated with CMake/CPack. A production package is an
explicit Release action, not part of the normal Debug development loop.

## Preconditions

- Run on a clean worktree at an exact `vMAJOR.MINOR.PATCH` Git tag.
- Build and test on each supported platform before publishing its artifact.
- Complete the applicable manual terminal-host verification items in `TODO.md`.
- Review the resolved vcpkg dependency set before public distribution.
  `THIRD_PARTY_NOTICES.md` is the inventory; CMake installs the reviewed full
  license texts under `share/doc/Vulpes/licenses`.

On Windows, run:

```powershell
.\scripts\dev.ps1 package
```

The command reconfigures from the tag, creates a Release build, runs the Release
tests, creates a ZIP archive in `out/packages`, and asks CPack to emit a
SHA-256 sidecar. It refuses a dirty or untagged worktree. Use an alternate
generated-artifact directory with `-OutputDirectory path\to\packages`.

## Artifact convention

Archives use this stable convention:

```text
vulpes-<version>-<os>-<architecture>.zip
vulpes-<version>-<os>-<architecture>.zip.sha256
```

For example, tag `v0.1.0` on the x64 Windows target produces
`vulpes-0.1.0-windows-x64.zip` and its SHA-256 sidecar. CMake derives the
version from the same tag used by `vulpes --version`; it does not accept a
second hand-maintained release version.

Each archive contains the executable, resolved non-system runtime libraries,
the `translations` directory, the Vulpes license, third-party notice inventory,
the complete reviewed third-party license bundle, and the operational/release
documentation. The archive is relocatable: unpack it as a directory and run
the executable from its `bin` folder. Windows archive consumers need the
supported Microsoft Visual C++ Redistributable; an installer that provisions it
is not part of the current archive process.

## Verification and publication

For every generated archive:

1. Verify its SHA-256 sidecar with the platform’s standard checksum tool.
2. Extract to a fresh directory with no build-tree DLLs on the executable path.
3. Run `vulpes --version`, `vulpes --terminal-capabilities`, and a workshop or
   inventory workflow from the extracted directory.
4. Verify that PDF export and localized catalog loading work from the archive.
5. Publish the archive, checksum, release notes, and full third-party license
   bundle together.

Windows code signing, macOS notarization, Linux distribution-native packages,
and a graphical installer are deliberately not claimed by this archive process.
They require platform credentials, host verification, and distribution policy
before the packaging TODO can be marked complete.

The exact public-release trust policy, supported package channels, clean-host
verification evidence, and exception rules are in
[distribution.md](distribution.md). The current ZIP is an unsigned pre-release
artifact until that policy is implemented and independently verified.
