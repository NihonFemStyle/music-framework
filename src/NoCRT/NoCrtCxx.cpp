// C++ runtime subset for the no-CRT build: global allocation functions and the
// failure hooks the header-only parts of the MSVC standard library call.
// Compiled with _HAS_EXCEPTIONS=0, so the library reports errors through these
// hooks instead of throwing.
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <new>
#include <string>

#include "NoCrtRuntime.h"

namespace
{
void* allocate(std::size_t size)
{
  void* memory = HeapAlloc(GetProcessHeap(), 0, size ? size : 1);
  if (!memory) mo_nocrt_fatal("out of memory");
  return memory;
}
void release(void* memory) noexcept
{
  if (memory) HeapFree(GetProcessHeap(), 0, memory);
}
void* allocateAligned(std::size_t size, std::align_val_t alignment)
{
  std::size_t align = static_cast<std::size_t>(alignment);
  if (align < sizeof(void*)) align = sizeof(void*);
  auto* raw = static_cast<unsigned char*>(allocate(size + align + sizeof(void*)));
  const std::uintptr_t user = (reinterpret_cast<std::uintptr_t>(raw + sizeof(void*)) + align - 1) & ~static_cast<std::uintptr_t>(align - 1);
  reinterpret_cast<void**>(user)[-1] = raw;
  return reinterpret_cast<void*>(user);
}
void releaseAligned(void* memory) noexcept
{
  if (memory) release(static_cast<void**>(memory)[-1]);
}
}

void* __cdecl operator new(std::size_t size) { return allocate(size); }
void* __cdecl operator new[](std::size_t size) { return allocate(size); }
void* __cdecl operator new(std::size_t size, const std::nothrow_t&) noexcept { return HeapAlloc(GetProcessHeap(), 0, size ? size : 1); }
void* __cdecl operator new[](std::size_t size, const std::nothrow_t&) noexcept { return HeapAlloc(GetProcessHeap(), 0, size ? size : 1); }
void __cdecl operator delete(void* memory) noexcept { release(memory); }
void __cdecl operator delete[](void* memory) noexcept { release(memory); }
void __cdecl operator delete(void* memory, std::size_t) noexcept { release(memory); }
void __cdecl operator delete[](void* memory, std::size_t) noexcept { release(memory); }
void __cdecl operator delete(void* memory, const std::nothrow_t&) noexcept { release(memory); }
void __cdecl operator delete[](void* memory, const std::nothrow_t&) noexcept { release(memory); }

void* __cdecl operator new(std::size_t size, std::align_val_t alignment) { return allocateAligned(size, alignment); }
void* __cdecl operator new[](std::size_t size, std::align_val_t alignment) { return allocateAligned(size, alignment); }
void __cdecl operator delete(void* memory, std::align_val_t) noexcept { releaseAligned(memory); }
void __cdecl operator delete[](void* memory, std::align_val_t) noexcept { releaseAligned(memory); }
void __cdecl operator delete(void* memory, std::size_t, std::align_val_t) noexcept { releaseAligned(memory); }
void __cdecl operator delete[](void* memory, std::size_t, std::align_val_t) noexcept { releaseAligned(memory); }

namespace std
{
[[noreturn]] void __cdecl _Xbad_alloc() { mo_nocrt_fatal("bad allocation"); }
[[noreturn]] void __cdecl _Xinvalid_argument(const char* message) { mo_nocrt_fatal(message); }
[[noreturn]] void __cdecl _Xlength_error(const char* message) { mo_nocrt_fatal(message); }
[[noreturn]] void __cdecl _Xout_of_range(const char* message) { mo_nocrt_fatal(message); }
[[noreturn]] void __cdecl _Xoverflow_error(const char* message) { mo_nocrt_fatal(message); }
[[noreturn]] void __cdecl _Xruntime_error(const char* message) { mo_nocrt_fatal(message); }
[[noreturn]] void __cdecl _Xbad_function_call() { mo_nocrt_fatal("bad function call"); }
#if !_HAS_EXCEPTIONS
// With exceptions disabled, std::exception::_Raise() consults this hook before
// failing fast through _invoke_watson (provided in NoCrtLibc.cpp).
void(__cdecl* _Raise_handler)(const exception&) = nullptr;
#endif
}
