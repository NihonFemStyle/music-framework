# Architecture

## Design goals

The code separates media discovery, state, presentation, graphics APIs, and the standalone window. This keeps Windows media logic independent from a host renderer and allows consumers to integrate only the pieces they need.

```text
Windows GSMTC
     │
     ▼
MediaSessionManager ──► OverlayState ──► FrameView
                                           │
                     ┌─────────────────────┼─────────────────────┐
                     ▼                     ▼                     ▼
                 Standalone          Integration API       Host controls
                     │                     │
                     └──────────── IRenderer abstraction ────────┘
                                           │
                              D3D9 / D3D10 / D3D11 / D3D12
```

## Core

`TrackInfo` is a renderer-independent snapshot containing metadata, playback state, timing, and encoded artwork bytes. `OverlayState` protects snapshot replacement with a slim reader/writer lock so asynchronous media updates and the window/render thread do not share mutable fields.

## Windows media adapter

`MediaSessionManager` uses `GlobalSystemMediaTransportControlsSessionManager` through C++/WinRT. It obtains the current system media session, reads media properties and artwork, and updates timeline state. The standalone host periodically polls in addition to responding to session changes so progress remains current.

## Rendering

`IRenderer` accepts a borrowed `NativeTarget` and immutable `FrameView`. The renderer never owns host devices, swap chains, or command queues.

`SoftwareRenderer` runs the same painter into a premultiplied BGRA buffer. The SDL3 host uses it to present through any SDL renderer (OpenGL, Vulkan, Direct3D 11/12, SDL GPU, software). It reuses the Windows media-session, spectrum, Discord, settings, and settings-window components, so it matches the native host. The native DirectX host and integration ABI remain independent of SDL.

The no-CRT build compiles the same library and standalone sources against `src/NoCRT`. Media discovery switches to `MediaSessionManagerAbi.cpp`, which uses the raw WinRT ABI. See [No-CRT](NOCRT.md).

The shared `OverlayPainter` uses Direct2D, DirectWrite, and WIC. It handles custom-family text fitting, geometry-clipped complementary text, album-art decoding, rounded artwork clipping, progress interpolation, dynamic accent extraction, media controls, and embedded branding.

## Standalone host

The Win32 application owns the overlay window, a graphics host for the selected backend (Direct3D 9Ex in a DWM-extended layered window, or a Direct3D 10.1, 11, or 12 DirectComposition swap chain, with Direct3D 11 as the fallback), notification-area icon, 450 ms double-tap shortcut state machine, native color/font dialogs, settings persistence, and the dedicated Direct2D settings window. It keeps `WS_EX_TRANSPARENT` enabled outside interaction mode and enables ordinary input automatically in Windowed Stream Mode.

## Settings

Settings are stored in:

```text
%LOCALAPPDATA%\NihonsMusicFramework\settings.ini
```

They include opacity, custom accent, installed font family, compact and Windowed Stream modes, artwork mode, dynamic accent, shortcut virtual-key codes, Overkill configuration, Discord RPC configuration, and renderer preference. Deleting this file restores defaults and triggers the first-run renderer prompt.

## Lifetime and threading

- WinRT media operations are asynchronous.
- `OverlayState` is the synchronization boundary.
- Rendering and Win32 interaction occur on the window thread.
- COM/WinRT is initialized before media or WIC use.
- Borrowed graphics objects must remain valid for the associated renderer lifetime.

