# Changelog

All notable changes to Nihon's Music Framework are documented here. The project follows semantic versioning.

## [Unreleased]

- Additional standalone Direct3D hosts
- A complete D3D12 command-recording contract
- Expanded no-CRT experimentation
- Future Linux support through MPRIS, PipeWire, and SDL3

## [1.4.0] - 2026-10-07

### Added

- Windowed Stream Mode for OBS and other broadcast tools, with interactive standard window behavior and a premultiplied-alpha DirectComposition surface for transparent capture
- Dedicated native Direct2D/DirectWrite settings window inspired by CyberiaStyle Last Approach, while retaining complete notification-area access
- Icon-only previous, play/pause, next, shuffle, and repeat controls while interaction mode is active
- GSMTC capability and playback-mode state tracking so unsupported media controls are visibly distinguished and never invoked
- Installed system-font selection for overlay titles and artist names through the settings window and notification-area menu
- Persisted overlay font family with Segoe UI Variable Display as the safe default

### Changed

- Reduced the interaction shortcut double-tap window from one second to 450 milliseconds to prevent accidental toggles
- Windowed Stream Mode now enables interaction automatically
- Media controls replace the banner while interaction mode is active and keep the progress bar clear of their hit targets
- Inactive media controls use a higher-contrast treatment for readability
- Text covered by artwork or spectrum geometry receives a complementary-color pass, including an inverted Chroma gradient, while unaffected text keeps its normal styling
- The custom font continues to use automatic text fitting in standard, compact, and Overkill layouts

### Fixed

- Preserved true transparency in OBS Window Capture instead of exposing a black composition background
- Corrected Windowed Stream Mode hit testing so mouse input no longer passes through the player
- Corrected the dedicated settings window title encoding
- Kept the overlay inside monitor bounds when mode or window dimensions change

## [1.3.2] - 2026-10-07

### Stashed fix

- Applied the transparency setting to the complete overlay, including artwork, text, and Overkill spectra, instead of only the background panel

## [1.3.1] - 2026-10-07

### Fixed

- Replaced the Discord IPC invalid-handle constant with a portable null disconnected state so MSVC Release builds compile correctly
- Corrected the Chroma rate unit to display as `°/s` during normal operation

### Easter eggs

- Preserved the malformed `Â°/s` Chroma rate label exclusively for April Fools' Day while using the correct `°/s` label normally

## [1.3.0] - 2026-10-07

### Added

- Configurable Overkill bar/point count, response, delay, maximum height, top fade, Chroma hue shifting, curve rendering, and artwork-masked spectra
- Optional Discord Rich Presence with track metadata, playback timestamps, configurable application/assets, and website/repository buttons

### Changed

- Spectrum curves now flow from the selected corner wall into the corresponding top or bottom screen edge
- Parenthetical title suffixes now move onto new lines, with a persisted option to hide them
- Artwork Spectrum now replaces the normal Overkill artwork tile instead of displaying the cover twice

### Fixed

- Restarted Overkill spectrum capture when the active audio-session process changes or capture fails
- Removed chroma-key outlines from Overkill text and spectrum bars while preserving opaque black artwork

## [1.2.2] - 2026-10-06

### Fixed

- Flattened malformed album-art alpha data during decoding so valid RGB pixels are no longer hidden in Overkill, tile, or artwork-background modes
- Limited the transparency setting to rendered panel elements so the layered window no longer makes album artwork and text translucent
- Replaced the black transparency key that incorrectly punched holes through dark album artwork

### Easter eggs

- Retained the source artwork's malformed alpha data on April Fools' Day

## [1.2.1] - 2026-10-06

### Fixed

- Added an opaque backing behind transparent artwork in tile and Overkill modes

### Easter eggs

- Removed the opaque artwork backing on April Fools' Day

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
