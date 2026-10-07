# Experimental no-CRT build

`MUSICOVERLAY_NOCRT` exists for developers who want a small, inspectable Win32 starting point without the Microsoft C/C++ runtime. It is not the normal application with a linker flag removed.

Build it with:

```powershell
.\build.ps1 -NoCRT
```

The executable uses:

- A custom `WinMainCRTStartup`
- `/NODEFAULTLIB`
- `/GS-`, `/GR-`, `/EHs-c-`, and `/Zl`
- Direct Win32/GDI calls
- The always-on-top and click-through window behavior
- Minimal local `memset` and `memcpy` primitives required when MSVC lowers initialization or copies to runtime symbols

Debug no-CRT configuration explicitly removes CMake's default `/RTC1` instrumentation because the corresponding `_RTC_*` hooks are provided by the CRT and are incompatible with `/NODEFAULTLIB`.

It intentionally omits:

- C++/WinRT media discovery
- STL containers and synchronized state
- Exceptions and RTTI
- DirectWrite/WIC rich UI
- Settings persistence and shortcut recording
- Tray integration
- Full integration-library behavior

These omissions are explicit because the full feature set relies on language/runtime facilities or would require carefully maintained replacements. Contributions that expand no-CRT support should remain isolated, document every runtime assumption, inspect linker imports, and avoid weakening the default build.

