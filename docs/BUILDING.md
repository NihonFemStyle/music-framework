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
| `-NoCRT` | Builds the full overlay and library without the C/C++ runtime | Off |
| `-SDL` | Also builds the SDL3 frontend (`musicoverlay_sdl.exe`) | Off |
| `-CLion` | Uses the matching CLion CMake preset and build tree | Off |
| `-CLionToolchain` | `MSBuild`, `MinGW` | `MSBuild` |
| `-Clean` | Removes the selected build tree before configuring | Off |
| `-Jobs` | Parallel build-job count | Logical processor count |

Examples:

```powershell
.\build.ps1 -Configuration Debug
.\build.ps1 -Architecture ARM64 -Configuration Release
.\build.ps1 -Clean
.\build.ps1 -NoCRT
.\build.ps1 -SDL
.\build.ps1 -CLion
.\build.ps1 -CLion -SDL -Configuration Debug
.\build.ps1 -CLion -CLionToolchain MinGW
```

Output follows this pattern:

```text
build-<architecture>-<default|sdl|nocrt>/<configuration>/musicoverlay_app.exe
```

The original Visual Studio generator path remains the default and requires no JetBrains software. With `-CLion`, output instead uses `cmake-build-clion-<architecture>-<default|sdl|nocrt>`.

## CLion

Open the repository root in a current CLion release and reload the CMake project. CLion detects `CMakePresets.json`. Every profile has explicit **Debug** and **Release** build presets. MSBuild profiles select the CLion toolchain named **Visual Studio** and cover Standard, SDL3, and No-CRT builds for x64, Win32, and ARM64. Separate Standard and SDL presets select the toolchain named **MinGW**; their architecture comes from that toolchain.

```powershell
.\build.ps1 -CLion
.\build.ps1 -CLion -Architecture Win32 -Configuration Debug
.\build.ps1 -CLion -Architecture ARM64 -NoCRT -Configuration MinSizeRel
.\build.ps1 -CLion -CLionToolchain MinGW
.\build.ps1 -CLion -CLionToolchain MinGW -SDL -Configuration Debug
```

CLion is optional. The default profiles use the installed Visual Studio 2022 MSVC and Windows SDK toolchain and do not replace the normal build workflow. The MinGW presets use the compiler architecture configured in CLion's MinGW toolchain. The No-CRT path remains MSVC-only.

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
| `MUSICOVERLAY_NOCRT` | `OFF` | Builds the overlay and library without the C/C++ runtime. See [No-CRT](NOCRT.md). |
| `MUSICOVERLAY_BUILD_SDL_FRONTEND` | `OFF` | Builds `musicoverlay_sdl`, which presents through any SDL renderer. |
| `MUSICOVERLAY_FETCH_SDL` | `ON` | Fetches the pinned SDL3 release when it is not installed. |

## SDL3 frontend

`musicoverlay_sdl.exe` has the same features as the native application: every layout and setting, media controls, the interaction shortcut, Windowed Stream Mode, Overkill spectra, Discord Rich Presence, the settings window, and a notification-area menu. It shares `settings.ini` with the native app. The overlay is drawn by the shared Direct2D painter into a CPU buffer, then presented through the selected SDL renderer, so it looks identical on every backend.

Choose the renderer in the tray menu (**SDL renderer**) or the settings window; the change takes effect after a restart. A command-line override applies to one session:

```powershell
.\build-x64-sdl\Release\musicoverlay_sdl.exe --renderer opengl
.\build-x64-sdl\Release\musicoverlay_sdl.exe --renderer vulkan
.\build-x64-sdl\Release\musicoverlay_sdl.exe --renderer direct3d11
.\build-x64-sdl\Release\musicoverlay_sdl.exe --renderer direct3d12
.\build-x64-sdl\Release\musicoverlay_sdl.exe --renderer gpu
.\build-x64-sdl\Release\musicoverlay_sdl.exe --renderer software
.\build-x64-sdl\Release\musicoverlay_sdl.exe --renderer auto
```

The default is OpenGL. If a requested renderer is unavailable, the frontend says so and falls back to SDL's default. Transparent overlay windows depend on the driver: OpenGL and Vulkan are the most reliable. If a Direct3D renderer shows an opaque background on your system, switch to one of those.

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

