# Building and packaging

## Toolchain

Nihon's Music Framework targets Windows and uses APIs supplied by the Windows SDK. The supported development configuration is Visual Studio 2022, MSVC, CMake 3.24+, and a current Windows 10 or 11 SDK.

Install the Visual Studio workload **Desktop development with C++**. No third-party package manager is required.

## Build script

The repository script creates a separate build tree for each architecture and runtime mode:

```powershell
.\build.ps1
```

Parameters:

| Parameter | Values | Default |
|---|---|---|
| `-Configuration` | `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel` | `Release` |
| `-Architecture` | `x64`, `Win32`, `ARM64` | `x64` |
| `-NoCRT` | Enables the experimental constrained no-CRT executable | Off |
| `-SDL` | Builds the optional SDL3 OpenGL/Vulkan frontend | Off |
| `-Clean` | Removes the selected build tree before configuring | Off |
| `-Jobs` | Parallel build-job count | Logical processor count |

Examples:

```powershell
.\build.ps1 -Configuration Debug
.\build.ps1 -Architecture ARM64 -Configuration Release
.\build.ps1 -Clean
.\build.ps1 -NoCRT
.\build.ps1 -SDL
```

Output follows this pattern:

```text
build-<architecture>-<default|nocrt>/<configuration>/musicoverlay_app.exe
```

## Direct CMake use

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

Useful options:

| Option | Default | Purpose |
|---|---:|---|
| `MUSICOVERLAY_BUILD_STANDALONE` | `ON` | Builds the desktop overlay. |
| `MUSICOVERLAY_BUILD_INTEGRATION` | `ON` | Exposes the integration interface target. |
| `MUSICOVERLAY_NOCRT` | `OFF` | Builds the minimal custom-entry-point executable. |
| `MUSICOVERLAY_BUILD_SDL_FRONTEND` | `OFF` | Builds `musicoverlay_sdl` with OpenGL and Vulkan runtime selection. |
| `MUSICOVERLAY_FETCH_SDL` | `ON` | Fetches the pinned SDL3 release when it is not installed. |

Run the portable frontend with one of these renderer selections:

```powershell
.\build-x64-sdl\Release\musicoverlay_sdl.exe --renderer opengl
.\build-x64-sdl\Release\musicoverlay_sdl.exe --renderer vulkan
```

The SDL frontend is currently Windows-first. Linux support, including MPRIS, PipeWire, Wayland/X11 behavior, and desktop integration, is planned for a future release.

## Runtime and resources

Non-Debug MSVC builds use the static MSVC runtime. The application icon, banner, and manifest are compiled into the executable. Windows components such as D3D, DirectWrite, WIC, DWM, and WinRT remain operating-system dependencies.

Debug builds use the debug DLL runtime and require the corresponding Visual Studio environment.

## Installing the library

```powershell
cmake --install build --config Release --prefix installed
```

This installs the static library, public headers, and exported `MusicOverlay::` CMake targets.

## Troubleshooting

- **No active track:** confirm the player appears in Windows media controls and is actively publishing a GSMTC session.
- **CMake cannot find MSVC:** install the Visual Studio C++ workload or run from a Developer PowerShell.
- **Old executable behavior:** rebuild after source/resource changes; embedded images and manifests only change when the executable is relinked.
- **No tray icon:** check the notification-area overflow menu and Windows notification settings.
- **Black or unsupported surface:** use the standalone D3D11 host first, then verify the integration backend's surface-format requirements.

