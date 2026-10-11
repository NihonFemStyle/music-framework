# No-CRT build

`MUSICOVERLAY_NOCRT` builds the complete overlay without the Microsoft C/C++ runtime. The executable imports only Windows system DLLs: no `VCRUNTIME`, `MSVCP`, or Universal CRT. It is the same application as the normal release, compiled from the same sources against a small runtime in `src/NoCRT`.

Build it with:

```powershell
.\build.ps1 -NoCRT
.\build.ps1 -NoCRT -Architecture Win32
.\build.ps1 -NoCRT -Architecture ARM64
```

From a Developer PowerShell, `build.ps1` checks the result with `dumpbin /dependents` and warns if any runtime DLL is imported. The pull-request workflow runs the same check.

## Feature coverage

| Feature | Normal | No-CRT |
|---|---|---|
| Media discovery (GSMTC), artwork, timeline | C++/WinRT | Raw WinRT ABI on a worker thread |
| Media controls (previous, play/pause, next, shuffle, repeat) | Yes | Yes |
| Direct2D/DirectWrite/WIC overlay painter, all layouts | Yes | Yes (shared code) |
| Compact, artwork background, dynamic accent, custom fonts | Yes | Yes |
| Overkill spectrum (process loopback capture, bars/curves, chroma, fade, delay, artwork spectrum) | Yes | Yes |
| Windowed Stream Mode with DirectComposition transparency | Yes | Yes |
| Click-through overlay and double-tap interaction shortcut | Yes | Yes |
| Settings persistence, settings window, tray menu, colour/font pickers | Yes | Yes |
| Discord Rich Presence | Yes | Yes |
| Integration library (`MusicOverlay::Integration`) | Yes | Yes (link `musicoverlay_nocrt_runtime`) |
| SDL3 frontend | Separate build | Not available: SDL3 and the C++/WinRT media service need the CRT |

## How it works

- **Compiler options:** `/GS- /GR- /EHs-c- /Zl /Gs1048576 /Zc:threadSafeInit-`. This means no security cookies, RTTI, exceptions, default-library directives, stack probes, or thread-safe static guards. `/RTC1`, `/EHsc`, and `/GR` are removed from CMake's defaults. All configurations use the static runtime selection, so `_DLL` and `_DEBUG` are never defined.
- **Linking:** `/NODEFAULTLIB` with a custom `wWinMainCRTStartup` entry point.
- **Standard library:** used header-only with `_HAS_EXCEPTIONS=0`, `_ITERATOR_DEBUG_LEVEL=0`, and `_USE_STD_VECTOR_ALGORITHMS=0`. Containers, strings, and algorithms are fine. `std::thread`, `std::mutex`, and `<chrono>` clocks are not used anywhere in the shared code. Threads use `CreateThread`, locks use `SRWLOCK`, and time comes from `GetTickCount64` and `GetSystemTimeAsFileTime`.
- **Runtime (`src/NoCRT`):**
  - `NoCrtLibc.cpp`: `mem*`, `strlen`/`wcslen`, `wcstol`, wide character classification, and `sqrt`/`sin`/`cos`/`exp`/`log`/`pow`/`fmod`/`floor`/`ceil`, plus `_fltused` and the CRT failure hooks. It includes no headers and is compiled with `/Oi-`.
  - `NoCrtCxx.cpp`: global `operator new`/`delete`, including the aligned and nothrow forms, on the process heap. Also the `std::_X*` error hooks the STL calls when exceptions are disabled.
  - `NoCrtStartup.cpp`: runs `.CRT$XI*`/`.CRT$XC*` static initializers and `atexit` destructors.
  - `NoCrtX86.cpp`: x86-only. Provides the 64-bit integer helpers (`_allmul`, `_alldiv`, `_aulldiv`, `_allrem`, `_aullrem`, `_alldvrm`, `_aulldvrm`, `_allshl`, `_allshr`, `_aullshr`) and the SSE2 conversion helpers (`_dtol3`, `_dtoul3`, `_dtoui3`, `_ftol3`, `_ftoul3`, `_ftoui3`, `_ltod3`, `_ultod3`, `_ltof3`, `_ultof3`). It uses only 32-bit arithmetic, so unoptimized builds cannot recurse into the helpers. x86 builds also compile with `/Oi-` so math calls are not replaced by CRT-only SSE2 entry points.
  - Shared code avoids `Interlocked*`; it uses SRW locks, which resolve on every architecture.
  - `NoCrtMain.cpp`: the entry point.
- **Media:** `MediaSessionManagerAbi.cpp` drives `GlobalSystemMediaTransportControlsSessionManager` through `RoGetActivationFactory` and the ABI interfaces. It polls `IAsyncInfo` instead of using coroutines and reads thumbnails through `CreateStreamOverRandomAccessStream`. The normal build keeps its C++/WinRT implementation.
- **Discord RPC:** framed IPC with complete writes, READY validation, and PING/PONG keep-alive. This follows the standalone DiscordRPC-NoCRT proof.

## Using the no-CRT integration library

A host executable that also avoids the CRT links `MusicOverlay::musicoverlay`. The `musicoverlay_nocrt_runtime` target comes along transitively. The host calls `mo_nocrt_initialize()` from its entry point before using C++ objects with dynamic initializers, and `mo_nocrt_finalize()` before `ExitProcess`. Hosts that link the normal CRT should use the normal library instead.

## Maintenance rules

Shared code must keep compiling in both modes:

- No exceptions, `try`/`catch`, `dynamic_cast`, or `typeid`.
- No `std::thread`, `std::mutex`, `std::condition_variable`, `<chrono>` clocks, iostreams, locales, or `std::function` in code compiled into the no-CRT targets.
- If a link error names a new CRT symbol, add a minimal implementation to the runtime rather than weakening the default build.
