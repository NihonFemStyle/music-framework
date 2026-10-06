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

`TrackInfo` is a renderer-independent snapshot containing metadata, playback state, timing, and encoded artwork bytes. `OverlayState` protects snapshot replacement with a mutex so asynchronous media updates and the window/render thread do not share mutable fields.

## Windows media adapter

`MediaSessionManager` uses `GlobalSystemMediaTransportControlsSessionManager` through C++/WinRT. It obtains the current system media session, reads media properties and artwork, and updates timeline state. The standalone host periodically polls in addition to responding to session changes so progress remains current.

## Rendering

`IRenderer` accepts a borrowed `NativeTarget` and immutable `FrameView`. The renderer never owns host devices, swap chains, or command queues.

The shared `OverlayPainter` uses Direct2D, DirectWrite, and WIC. It handles text fitting, album-art decoding, rounded artwork clipping, progress interpolation, dynamic accent extraction, settings controls, and embedded branding.

## Standalone host

The Win32 application owns the overlay window, D3D11 device and swap chain, notification-area icon, double-tap shortcut state machine, native color dialog, settings persistence, and window animations. It keeps `WS_EX_TRANSPARENT` enabled outside interaction mode.

## Settings

Settings are stored in:

```text
%LOCALAPPDATA%\NihonsMusicFramework\settings.ini
```

They include opacity, custom accent, compact mode, artwork mode, dynamic accent, shortcut virtual-key codes, and renderer preference. Deleting this file restores defaults and triggers the first-run renderer prompt.

## Lifetime and threading

- WinRT media operations are asynchronous.
- `OverlayState` is the synchronization boundary.
- Rendering and Win32 interaction occur on the window thread.
- COM/WinRT is initialized before media or WIC use.
- Borrowed graphics objects must remain valid for the associated renderer lifetime.

