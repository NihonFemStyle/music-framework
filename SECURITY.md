# Security Policy

## Supported Versions

Security updates are provided for the latest stable release:

| Version | Supported |
|---|---|
| 1.x (latest release) | :white_check_mark: |
| Older releases | :x: |

Always use the latest stable release to receive current security fixes and reliability improvements.

## Reporting a Vulnerability

The maintainers of Nihon's Music Framework take security vulnerabilities seriously and appreciate responsible disclosure.

**Do not report security vulnerabilities through public GitHub issues.**

Report them directly to **security@arch-linux.pro**. Include:

- **Affected version or commit:** release tag or full commit SHA
- **Vulnerability type:** memory safety, malformed media data, COM lifetime, graphics resource state, unsafe integration pointer, packaging, or another category
- **Impact:** realistic consequences and required privileges
- **Reproduction steps:** the smallest reliable sequence
- **Proof of concept:** a minimal non-destructive example, when appropriate
- **Environment:** Windows version, architecture, build configuration, media source, and DirectX backend
- **Suggested fix:** optional remediation ideas

Remove personal media metadata, credentials, tokens, and unrelated private data from screenshots, logs, dumps, or samples.

## What to Expect

- **Acknowledgment:** within 48 hours
- **Initial assessment:** within 5 business days
- **Progress updates:** at least every 7 days while active
- **Target resolution:** critical vulnerabilities within 30 days when practical

Timelines may change when a report depends on Microsoft platform behavior, hardware/vendor drivers, or coordinated fixes. The maintainers will communicate material changes.

## Coordinated Disclosure

- The maintainers will work with the reporter to determine scope and severity.
- Details will not be intentionally published before a fix or mitigation is available.
- Reporters may receive a candidate fix for verification.
- Credit will be included in an advisory unless anonymity is requested.

## Project-Specific Security Considerations

Nihon's Music Framework is a local Windows desktop application and integration library. It does not provide authentication, authorization, a network service, or cloud data storage.

Security-sensitive boundaries include:

- Artwork and metadata bytes supplied by Windows media sessions
- WIC image decoding and DirectWrite text layout
- Asynchronous C++/WinRT session callbacks and object lifetimes
- Borrowed device, swap-chain, command-queue, and window pointers supplied by integration hosts
- D3D resource ownership, resize ordering, and synchronization
- Global keyboard-state polling used for the configurable interaction shortcut
- Local settings stored in `%LOCALAPPDATA%\NihonsMusicFramework\settings.ini`
- Embedded Windows resources and release packaging

Integration consumers are responsible for validating pointer lifetimes, preserving graphics resource states, and ensuring that injected or third-party host environments authorize use of the library.

## Secure Development Practices

Contributors should:

- Never commit credentials, signing keys, tokens, private crash dumps, or personal media history.
- Validate lengths, dimensions, and conversions before processing external metadata or artwork.
- Use RAII for COM and graphics resources.
- Treat `NativeTarget` pointers as borrowed and never release host-owned objects.
- Document command-list, backbuffer-state, descriptor, and fence assumptions for D3D12 work.
- Avoid weakening compiler or linker protections in the default build.
- Keep no-CRT experiments isolated from the normal application.
- Test malformed or missing artwork and unusually long metadata.

## Security Updates

Confirmed fixes will be documented in release notes and, when appropriate, a GitHub Security Advisory for [NihonFemStyle/music-framework](https://github.com/NihonFemStyle/music-framework). Backports are not guaranteed; users should upgrade to the latest release.

## Hall of Fame

Researchers who responsibly disclose vulnerabilities may be listed here with permission.

_No entries yet._

## Contact

- **Security:** security@arch-linux.pro
- **General:** contact@arch-linux.pro
- **Repository:** https://github.com/NihonFemStyle/music-framework

Last updated: October 6, 2026.

