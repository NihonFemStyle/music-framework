# Contributing to Nihon's Music Framework

Thanks for your interest in contributing. Code, documentation, bug reports, feature requests, design feedback, and renderer research are all welcome.

Before participating, read the [Code of Conduct](CODE_OF_CONDUCT.md). Security vulnerabilities belong in the private process described by [SECURITY.md](SECURITY.md), not in public issues.

## Getting Started

### 1. Fork and clone

Fork [NihonFemStyle/music-framework](https://github.com/NihonFemStyle/music-framework), then clone your fork:

```bash
git clone https://github.com/YOUR-USERNAME/music-framework.git
cd music-framework
```

### 2. Create a focused branch

```bash
git switch -c feature/short-description
```

Use prefixes such as `feature/`, `fix/`, `docs/`, or `refactor/` when useful.

### 3. Build before editing

```powershell
.\build.ps1 -Configuration Debug -Clean
```

See [docs/BUILDING.md](docs/BUILDING.md) for prerequisites and additional configurations.

### 4. Make and verify the change

- Follow the existing module boundaries and C++ style.
- Keep commits focused and use descriptive messages.
- Update user and developer documentation when behavior changes.
- Avoid adding third-party GUI frameworks.
- Explain any new dependency and keep it optional when practical.

Recommended commit subjects include `fix: preserve artwork aspect ratio` and `feat: add host resize contract`.

### 5. Push and open a pull request

```bash
git push -u origin feature/short-description
```

Open a pull request against the `main` branch at [github.com/NihonFemStyle/music-framework](https://github.com/NihonFemStyle/music-framework). Complete the pull-request template and describe tested configurations and known limitations.

## Ways to Contribute

- **Bug reports:** use the [bug-report form](https://github.com/NihonFemStyle/music-framework/issues/new?template=bug_report.yml).
- **Feature requests:** use the [feature-request form](https://github.com/NihonFemStyle/music-framework/issues/new?template=feature_request.yml).
- **Documentation:** correct unclear instructions, examples, compatibility claims, or diagrams.
- **Code:** improve media handling, UI behavior, renderer implementations, integration contracts, packaging, accessibility, and diagnostics.
- **Testing:** report Windows, GPU, DPI, media-player, and DirectX compatibility results.

## Coding Guidelines

- Use C++20 and native Windows APIs.
- Prefer RAII and `Microsoft::WRL::ComPtr` for owned COM references.
- Keep public headers under `include/MusicOverlay`.
- Keep media state independent of renderer-specific objects.
- Treat everything in `NativeTarget` as borrowed; the host retains ownership.
- Preserve click-through behavior outside interactive mode.
- Keep no-CRT work explicitly gated and separate from the default feature-complete build.
- Comment assumptions and non-obvious lifetime or synchronization rules, not routine syntax.

## Verification Checklist

Build the affected configurations:

```powershell
.\build.ps1 -Configuration Debug
.\build.ps1 -Configuration Release
```

For UI or standalone changes, check:

- Normal and compact layouts
- Artwork tile, artwork background, and missing-artwork states
- Long titles and artist names
- Dynamic and custom accent colors
- 10% and 100% opacity boundaries
- Double-tap activation, custom key combinations, drag behavior, and tray commands
- Settings open/close animation and persistence across restart
- High-DPI and multi-monitor behavior when relevant

For renderer changes, document:

- API and feature level
- Swap-chain and backbuffer format
- Resize and device-loss behavior
- Resource ownership and lifetime
- D3D12 state-transition, descriptor, command-list, queue, and fence assumptions

## Pull Request Expectations

A pull request should explain:

1. The problem or use case
2. The chosen design
3. User-visible and public-API changes
4. Verification performed
5. Compatibility impact and remaining limitations

Small, cohesive pull requests are easier to review than unrelated bundles.

## License

By submitting a contribution, you agree that it may be distributed under the project's [MIT License](LICENSE).

Thanks for helping improve Nihon's Music Framework.
