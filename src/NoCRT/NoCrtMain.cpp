// Entry point for the no-CRT standalone executable.
//
// The no-CRT build compiles the same standalone application as the normal
// release (src/Standalone/WinMain.cpp) against the minimal runtime in this
// directory. This file replaces the CRT startup: it runs static initializers,
// calls wWinMain, runs atexit handlers, and exits the process.
#include <windows.h>

#include "NoCrtRuntime.h"

// Defined in src/Standalone/WinMain.cpp.
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, LPWSTR commandLine, int show);

extern "C" void __cdecl wWinMainCRTStartup(void)
{
  mo_nocrt_initialize();
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  GetStartupInfoW(&startup);
  const int show = (startup.dwFlags & STARTF_USESHOWWINDOW) ? startup.wShowWindow : SW_SHOWDEFAULT;
  const int code = wWinMain(GetModuleHandleW(nullptr), nullptr, GetCommandLineW(), show);
  mo_nocrt_finalize();
  ExitProcess(static_cast<UINT>(code));
}
