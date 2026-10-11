#pragma once
// Minimal runtime support for the no-CRT build.
//
// The no-CRT build links with /NODEFAULTLIB, so nothing from vcruntime, the
// Universal CRT, or the C++ standard library binaries is available. This
// runtime supplies the small set of symbols the compiler and the header-only
// parts of the standard library expect: memory/string primitives, the math
// functions used by the overlay, operator new/delete, the STL failure hooks,
// static initialization/termination, and (on x86) compiler conversion helpers.
//
// Hosts that embed the no-CRT integration library in their own no-CRT
// executable call mo_nocrt_initialize() before using any C++ object with a
// dynamic initializer, and mo_nocrt_finalize() before ExitProcess.

#ifdef __cplusplus
extern "C" {
#endif

void __cdecl mo_nocrt_initialize(void);
void __cdecl mo_nocrt_finalize(void);
__declspec(noreturn) void __cdecl mo_nocrt_fatal(const char* reason);

#ifdef __cplusplus
}
#endif
