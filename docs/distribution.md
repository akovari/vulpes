# Distribution trust policy

This document defines how Vulpes will distribute a public release. It is a
release gate, not an implementation of signing credentials or an authorization
to publish an unsigned build.

The current CPack ZIP plus SHA-256 sidecar is a developer and pre-release
artifact. A checksum detects accidental corruption after a user obtains a
file; it does not identify the publisher. Do not describe an archive as
signed, notarized, or production-ready until the platform-specific gates below
have passed for that exact artifact.

## Common release controls

Every public artifact must start from a clean, exact `vMAJOR.MINOR.PATCH` tag.
Build it natively for its target, run its release tests, and extract it into a
fresh directory outside the build tree. Keep the archive, SHA-256 sidecar,
signature or notarization evidence, dependency/license review, and release
notes together under the same version.

The signing identity, private key, Apple credentials, timestamp credentials,
and repository signing keys never belong in Git, the SQLite application file,
or a release archive. Store them in the platform key store, a hardware-backed
token, or an approved secret service with least-privilege access. Record the
public certificate subject/key fingerprint and the verification log with the
release so an independent maintainer can audit it without accessing secrets.

A clean test host must verify the package before publication. At minimum it
must run `vulpes --version`, `vulpes --terminal-capabilities`, the terminal
diagnostics key/resize checks, a workshop or inventory browse/edit workflow,
localized catalog loading, and PDF export. The terminal matrix in `TODO.md`
remains a separate gate: package validation cannot substitute for real terminal
host behavior.

## Windows

Windows public artifacts will contain an Authenticode-signed `vulpes.exe` and,
once Vulpes offers an installer, an independently signed installer package.
The native installer decision is intentionally deferred until there is a
supported update/uninstall model; the portable ZIP remains the first delivery
format. A winget manifest may only refer to a signed, verified installer or
portable release and its published hash.

Use Microsoft's current `signtool` with a trusted code-signing identity, a
SHA-256 file digest, and an RFC 3161 SHA-256 timestamp. Verify the released
binary and any installer on a clean Windows host with `signtool verify /pa
/all`. Archive extraction must prove that the signed executable still verifies
and runs without DLLs from the build tree. Follow the current
[SignTool guidance](https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool)
and [Authenticode timestamp guidance](https://learn.microsoft.com/en-us/windows/win32/seccrypto/time-stamping-authenticode-signatures)
rather than freezing certificate-provider details in this repository.

Before the first Windows public release, verify the extracted artifact in
Windows Terminal and every Windows console host Vulpes claims to support. Test
Escape, arrows, Ctrl+C, function keys, Alt mnemonics, resize, UTF-8 input, and
terminal restoration with `--terminal-diagnostics`.

## macOS

Build macOS artifacts natively for each supported architecture. Sign all
distributed executable code and bundled non-system libraries with a Developer
ID identity, hardened runtime, and secure timestamp before packaging. The
production native package will be a signed, notarized installer package or
disk image; choose one only after its install and uninstall behavior is tested.
The portable archive may accompany it, but must not be presented as a native
installer.

Submit the final signed deliverable to Apple's notary service using the current
`notarytool` workflow, staple the accepted ticket where the chosen package
format supports stapling, and retain the submission log. Verify the signature
and Gatekeeper assessment on a clean Mac that has not previously run Vulpes.
Apple requires Developer ID signing, a hardened runtime, and a secure timestamp
for modern notarized direct distribution; see
[Notarizing macOS software before distribution](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution)
and [creating distribution-signed code](https://developer.apple.com/documentation/xcode/creating-distribution-signed-code-for-the-mac).

The clean-host acceptance includes Terminal and iTerm (and SSH when claimed),
with Escape ambiguity, Ctrl+C, resize, UTF-8, and terminal restoration checked
through Vulpes diagnostics before publication.

## Linux

Linux delivery starts with an architecture-specific portable archive, SHA-256
sidecar, and detached release signature from a documented project signing key.
The checksum is retained for routine integrity checks; the detached signature
is the publisher-authentication mechanism. Publish its verification command
and the signing-key fingerprint in the release notes.

Native packages are a later, separately verified channel:

- Debian and Ubuntu packages are distributed through a repository whose
  `Release`/`InRelease` metadata is signed and installed with an explicit
  `Signed-By` keyring.
- RPM packages are signed with the project RPM signing key and verified with
  the platform's RPM signature check before installation.
- Flatpak, Snap, or another ecosystem is considered only when its maintenance,
  sandboxing, update, and signing model has been reviewed; it is not implied by
  the portable archive.

APT repository authentication is defined by
[apt-secure](https://manpages.debian.org/testing/apt/apt-secure.8.en.html).
Choose and document the supported distribution versions before publishing any
native package. Test every package in a clean native container or VM, then
exercise the terminal matrix in an actual supported host; container execution
does not verify raw terminal restoration.

## Evidence and exceptions

Publish only after the release owner records:

1. exact Git tag and commit;
2. native builder and target architecture;
3. archive and detached-signature/checksum hashes;
4. certificate or key fingerprint, signature verification output, and, on
   macOS, notarization submission and assessment output;
5. clean-host install/extraction and functional-test result; and
6. completed terminal-host matrix entries in `TODO.md`.

An exception must be a conspicuous pre-release label approved by the release
owner and must state which normal trust guarantee is absent. An unsigned build
is never silently promoted into a public release.
