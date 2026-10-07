# Changelog

All notable changes to Nihon's Music Framework are documented here. The project follows semantic versioning.

## [Unreleased]

- Additional standalone Direct3D hosts
- A complete D3D12 command-recording contract
- Expanded no-CRT experimentation
- Future Linux support through MPRIS, PipeWire, and SDL3

## [1.2.1] - 2026-10-06

### Fixed

- Made transparent album-art pixels render against an opaque backing in tile and Overkill modes

### Easter eggs

- Preserved transparent album artwork without its backing on April Fools' Day

## [1.2.0] - 2026-10-06

### Added

- Optional SDL3 standalone frontend for Windows
- Runtime-selectable OpenGL and Vulkan rendering through `--renderer`
- `-SDL` PowerShell build option and isolated SDL build directory
- Pinned, statically linked SDL 3.4.16 dependency support through CMake
- Complete settings access from the notification-area menu
- Nested tray submenus for transparency, renderer preference, and Overkill corner
- SDL build matrices for pull requests, cutting-edge artifacts, and versioned releases
- Dependency caching for SDL GitHub Actions jobs

### Changed

- Expanded the tray menu with compact mode, accent controls, artwork mode, shortcut recording, Overkill controls, and banner visibility
- Updated architecture and build documentation for the SDL, OpenGL, and Vulkan frontend
- Marked Linux as future platform work and macOS as unsupported

## [1.1.1] - 2026-10-06

### Fixed

- Corrected featured-artist separator parsing and display
- Prevented the featured-artist separator from being corrupted by source-file encoding

### Changed

- Added the Overkill mode screenshot to the README preview gallery
- Preserved the corrupted separator as an April Fools' Day easter egg

## [1.1.0] - 2026-10-06

### Added

- Overkill mode with a 32-band, source-responsive audio spectrum
- Process-isolated Windows WASAPI loopback capture for the active media application
- Four selectable, persistent Overkill screen corners
- Full-monitor corner placement while a foreground application is fullscreen
- Transparent Overkill presentation with edge-oriented spectrum bars
- Optional persistent banner visibility
- Automatic title cleanup that moves featured performers to the artist line
- Title line breaks for version, remix, VIP, dash, and pipe separators
- Screen-boundary constraints for dragging and the animated settings panel

### Changed

- Moved settings access entirely to the notification-area icon
- Locked Overkill mode to its selected display corner
- Updated the README with Overkill behavior, capture limitations, and requirements

### Fixed

- Replaced unsupported process-loopback `GetMixFormat()` usage with an explicit PCM capture format
- Improved process-to-audio-session matching and spectrum responsiveness
- Prevented settings from extending beyond the monitor work area

## [1.0.0] - 2026-10-06

### Added

- Native standalone Direct3D 11 overlay
- Windows GSMTC media discovery and artwork loading
- D3D9, D3D10, D3D11, and D3D12 integration abstractions
- Direct2D, DirectWrite, and WIC presentation layer
- Smooth progress interpolation and dynamic artwork accent
- Normal, compact, tile-artwork, and background-artwork modes
- Animated settings panel and persistent settings
- Configurable double-tap keyboard shortcut recorder
- Native color picker, opacity control, DWM blur, and drag movement
- Notification-area menu with settings, interaction, and exit commands
- Embedded application icon, banner, and Windows manifest
- Experimental constrained no-CRT executable
- CMake build/install support and PowerShell build script

### Documentation

- Added build, architecture, integration, no-CRT, contributing, security, and community documentation
- Added GitHub issue templates, pull-request template, Windows CI, and preview gallery
