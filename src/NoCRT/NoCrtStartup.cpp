// Static initialization and termination for the no-CRT build.
//
// The compiler places pointers to dynamic initializers in the .CRT$XC? (C++)
// and .CRT$XI? (C) sections and registers destructors through atexit(). The
// linker sorts those sections alphabetically, so the markers below bracket the
// compiler's entries exactly as the real CRT does.
#include <windows.h>

#include "NoCrtRuntime.h"

typedef void(__cdecl* MoInitializer)(void);
typedef int(__cdecl* MoInitializerWithResult)(void);

#pragma section(".CRT$XIA", long, read)
#pragma section(".CRT$XIZ", long, read)
#pragma section(".CRT$XCA", long, read)
#pragma section(".CRT$XCZ", long, read)

extern "C" {
__declspec(allocate(".CRT$XIA")) MoInitializerWithResult mo_xi_a[] = {nullptr};
__declspec(allocate(".CRT$XIZ")) MoInitializerWithResult mo_xi_z[] = {nullptr};
__declspec(allocate(".CRT$XCA")) MoInitializer mo_xc_a[] = {nullptr};
__declspec(allocate(".CRT$XCZ")) MoInitializer mo_xc_z[] = {nullptr};
}

#pragma comment(linker, "/merge:.CRT=.rdata")

namespace
{
constexpr int kMaxExitHandlers = 1024;
MoInitializer g_exitHandlers[kMaxExitHandlers];
int g_exitHandlerCount = 0;
bool g_initialized = false;
}

extern "C" int __cdecl atexit(void(__cdecl* handler)(void))
{
  if (!handler || g_exitHandlerCount >= kMaxExitHandlers) return -1;
  g_exitHandlers[g_exitHandlerCount++] = handler;
  return 0;
}

extern "C" void __cdecl mo_nocrt_initialize(void)
{
  if (g_initialized) return;
  g_initialized = true;
  for (MoInitializerWithResult* entry = mo_xi_a; entry < mo_xi_z; ++entry)
    if (*entry && (*entry)() != 0) mo_nocrt_fatal("C initializer failed");
  for (MoInitializer* entry = mo_xc_a; entry < mo_xc_z; ++entry)
    if (*entry) (*entry)();
}

extern "C" void __cdecl mo_nocrt_finalize(void)
{
  while (g_exitHandlerCount > 0)
  {
    MoInitializer handler = g_exitHandlers[--g_exitHandlerCount];
    if (handler) handler();
  }
}

extern "C" __declspec(noreturn) void __cdecl mo_nocrt_fatal(const char* reason)
{
  OutputDebugStringA("Nihon's Music Framework (no-CRT) fatal error: ");
  OutputDebugStringA(reason ? reason : "unknown");
  OutputDebugStringA("\n");
  if (IsDebuggerPresent()) DebugBreak();
  ExitProcess(3);
}
