cmake_minimum_required(VERSION 3.24)
project(NihonsMusicFramework VERSION 2.0.0 LANGUAGES C CXX)

include(GNUInstallDirs)

if(NOT WIN32)
  message(FATAL_ERROR "Nihon's Music Framework is a native Windows project")
endif()

option(MUSICOVERLAY_BUILD_STANDALONE "Build the desktop overlay" ON)
option(MUSICOVERLAY_BUILD_INTEGRATION "Build the reusable integration library" ON)
option(MUSICOVERLAY_NOCRT "Build the overlay and library without the C/C++ runtime (MSVC)" OFF)
option(MUSICOVERLAY_BUILD_SDL_FRONTEND "Build the SDL3 frontend (all SDL renderers)" OFF)
option(MUSICOVERLAY_FETCH_SDL "Fetch SDL3 when it is unavailable" ON)

if(MUSICOVERLAY_NOCRT AND NOT MSVC)
  message(FATAL_ERROR "MUSICOVERLAY_NOCRT currently supports MSVC only")
endif()

if(MUSICOVERLAY_BUILD_SDL_FRONTEND AND MUSICOVERLAY_NOCRT)
  message(FATAL_ERROR "The SDL frontend links SDL3 and the C++/WinRT media service, which require the CRT. Configure the SDL frontend in a separate build tree.")
endif()

# ---------------------------------------------------------------------------
# No-CRT mode
#
# Everything (library, standalone app) is compiled without exceptions, RTTI,
# security cookies, stack probes, or thread-safe statics, and linked with
# /NODEFAULTLIB against the small runtime in src/NoCRT. The standard library is
# used header-only with _HAS_EXCEPTIONS=0; media discovery uses the raw WinRT
# ABI instead of C++/WinRT.
# ---------------------------------------------------------------------------
if(MUSICOVERLAY_NOCRT)
  foreach(flag_var
      CMAKE_C_FLAGS CMAKE_CXX_FLAGS
      CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG
      CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELEASE
      CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_RELWITHDEBINFO
      CMAKE_C_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_MINSIZEREL)
    # /RTC1 needs _RTC_* hooks from the CRT; /EHsc and /GR pull in EH and RTTI support.
    string(REPLACE "/RTC1" "" ${flag_var} "${${flag_var}}")
    string(REPLACE "/EHsc" "" ${flag_var} "${${flag_var}}")
    string(REGEX REPLACE "/GR( |$)" " " ${flag_var} "${${flag_var}}")
  endforeach()
  # Static runtime selection in every configuration: no _DLL (no dllimport CRT
  # declarations) and no _DEBUG (no debug CRT reporting hooks). /Zl strips the
  # default-library directives this would otherwise add.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

set(MO_NOCRT_DEFINITIONS
  MUSICOVERLAY_NOCRT=1
  MUSICOVERLAY_MEDIA_ABI=1
  _HAS_EXCEPTIONS=0
  _ITERATOR_DEBUG_LEVEL=0
  _USE_STD_VECTOR_ALGORITHMS=0
  _CRT_SECURE_NO_WARNINGS)
set(MO_NOCRT_FLAGS /GS- /GR- /EHs-c- /Zl /Gs1048576 /Zc:threadSafeInit- /sdl- /permissive- /Zc:__cplusplus)
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  # 32-bit x86: with intrinsics enabled, MSVC replaces sin/cos/pow/exp/sqrt/fmod
  # calls with CRT-only SSE2 entry points (__libm_sse2_*_precise, _CIfmod).
  # Disabling the function-form intrinsics keeps them as ordinary calls into
  # src/NoCRT/NoCrtLibc.cpp. Interlocked and other intrinsic-only functions are unaffected.
  list(APPEND MO_NOCRT_FLAGS /Oi-)
endif()
# Restricted to C/C++ so the options never reach the resource compiler.
set(MO_NOCRT_OPTIONS)
foreach(flag IN LISTS MO_NOCRT_FLAGS)
  list(APPEND MO_NOCRT_OPTIONS "$<$<COMPILE_LANGUAGE:C,CXX>:${flag}>")
endforeach()

if(MUSICOVERLAY_NOCRT)
  add_library(musicoverlay_nocrt_runtime STATIC
    src/NoCRT/NoCrtRuntime.h
    src/NoCRT/NoCrtLibc.cpp
    src/NoCRT/NoCrtCxx.cpp
    src/NoCRT/NoCrtStartup.cpp
    src/NoCRT/NoCrtX86.cpp)
  target_compile_features(musicoverlay_nocrt_runtime PRIVATE cxx_std_20)
  target_compile_definitions(musicoverlay_nocrt_runtime PUBLIC ${MO_NOCRT_DEFINITIONS} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
  target_compile_options(musicoverlay_nocrt_runtime PRIVATE ${MO_NOCRT_OPTIONS})
  # The libc file must not have its loops recognized and replaced by calls to itself.
  set_source_files_properties(src/NoCRT/NoCrtLibc.cpp PROPERTIES COMPILE_OPTIONS "/Oi-")
  target_include_directories(musicoverlay_nocrt_runtime PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/NoCRT>)
  # On 32-bit x86 the 64-bit integer and conversion helpers (_alldiv, _allmul,
  # _aulldvrm, _allshl, _dtol3, _ultof3, ...) are implemented in NoCrtX86.cpp.
  target_link_libraries(musicoverlay_nocrt_runtime PUBLIC kernel32 user32)
endif()

# ---------------------------------------------------------------------------
# Core library (media, spectrum, renderers, painter)
# ---------------------------------------------------------------------------
set(MO_PUBLIC_HEADERS
  include/MusicOverlay/Core/TrackInfo.h
  include/MusicOverlay/Core/OverlayState.h
  include/MusicOverlay/Renderer/IRenderer.h
  include/MusicOverlay/Renderer/SoftwareRenderer.h
  include/MusicOverlay/Integration/MusicOverlay.h)

set(MO_LIBRARY_SOURCES
  ${MO_PUBLIC_HEADERS}
  src/Core/OverlayState.cpp
  src/Core/MetadataFormatter.cpp
  src/Core/MetadataFormatter.h
  src/Platform/Windows/MediaSessionManager.h
  src/Platform/Windows/ProcessSpectrumCapture.cpp
  src/Platform/Windows/ProcessSpectrumCapture.h
  src/Renderer/RendererFactory.cpp
  src/Renderer/D3D9Renderer.cpp
  src/Renderer/D3D10Renderer.cpp
  src/Renderer/D3D11Renderer.cpp
  src/Renderer/D3D12Renderer.cpp
  src/Renderer/SoftwareRenderer.cpp
  src/Renderer/Renderers.h
  src/UI/OverlayPainter.cpp
  src/UI/OverlayPainter.h)
if(MUSICOVERLAY_NOCRT)
  list(APPEND MO_LIBRARY_SOURCES src/Platform/Windows/MediaSessionManagerAbi.cpp)
else()
  list(APPEND MO_LIBRARY_SOURCES src/Platform/Windows/MediaSessionManager.cpp)
endif()

# The standalone executables need the library, so it is always built; the
# MUSICOVERLAY_BUILD_INTEGRATION option controls the exported integration target.
add_library(musicoverlay STATIC ${MO_LIBRARY_SOURCES})
target_include_directories(musicoverlay
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    src)
target_compile_features(musicoverlay PUBLIC cxx_std_20)
target_compile_definitions(musicoverlay PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(musicoverlay PUBLIC d3d9 d3d10 d3d11 d3d12 dxgi d2d1 dwrite windowscodecs runtimeobject mmdevapi ole32)

if(MUSICOVERLAY_NOCRT)
  target_compile_options(musicoverlay PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4> ${MO_NOCRT_OPTIONS})
  # Runtime-free consumers must use the same STL configuration, so the no-CRT
  # definitions and runtime propagate through the runtime target.
  target_link_libraries(musicoverlay PUBLIC musicoverlay_nocrt_runtime shcore uuid)
elseif(MSVC)
  target_compile_options(musicoverlay PRIVATE /W4 /permissive- /EHsc /Zc:__cplusplus)
  set_property(TARGET musicoverlay PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:DebugDLL>")
endif()

# ---------------------------------------------------------------------------
# Native standalone overlay (normal and no-CRT builds share the same sources)
# ---------------------------------------------------------------------------
set(MO_STANDALONE_SHARED_SOURCES
  src/Standalone/SettingsWindow.cpp
  src/Standalone/SettingsWindow.h
  src/Standalone/StandaloneSettings.cpp
  src/Standalone/StandaloneSettings.h
  src/Platform/Windows/DiscordRpc.cpp
  src/Platform/Windows/DiscordRpc.h
  src/Standalone/MusicOverlay.rc
  src/Standalone/app.manifest
  src/Standalone/resource.h)

if(MUSICOVERLAY_BUILD_STANDALONE)
  add_executable(musicoverlay_app WIN32
    src/Standalone/WinMain.cpp
    src/Standalone/GraphicsHosts.cpp
    src/Standalone/GraphicsHosts.h
    ${MO_STANDALONE_SHARED_SOURCES})
  target_include_directories(musicoverlay_app PRIVATE src)
  target_compile_definitions(musicoverlay_app PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
  target_link_libraries(musicoverlay_app PRIVATE musicoverlay d3d10_1 user32 gdi32 dwmapi dcomp shell32 comdlg32 comctl32)
  if(MUSICOVERLAY_NOCRT)
    target_sources(musicoverlay_app PRIVATE src/NoCRT/NoCrtMain.cpp)
    target_compile_options(musicoverlay_app PRIVATE ${MO_NOCRT_OPTIONS})
    target_link_options(musicoverlay_app PRIVATE /NODEFAULTLIB /ENTRY:wWinMainCRTStartup)
    target_link_libraries(musicoverlay_app PRIVATE kernel32)
  elseif(MSVC)
    set_property(TARGET musicoverlay_app PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:DebugDLL>")
  endif()
endif()

# ---------------------------------------------------------------------------
# SDL3 frontend
# ---------------------------------------------------------------------------
if(MUSICOVERLAY_BUILD_SDL_FRONTEND)
  if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:DebugDLL>")
  endif()
  find_package(SDL3 CONFIG QUIET)
  if(NOT SDL3_FOUND AND MUSICOVERLAY_FETCH_SDL)
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3 URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(SDL3)
    set(SDL3_DIR "${sdl3_BINARY_DIR}" CACHE PATH "SDL3 package directory" FORCE)
    if(TARGET SDL3-static AND NOT TARGET SDL3::SDL3)
      add_library(SDL3::SDL3 ALIAS SDL3-static)
    endif()
  endif()
  if(NOT TARGET SDL3::SDL3)
    message(FATAL_ERROR "SDL3 is required for MUSICOVERLAY_BUILD_SDL_FRONTEND")
  endif()
  add_executable(musicoverlay_sdl WIN32
    src/Standalone/SDLMain.cpp
    ${MO_STANDALONE_SHARED_SOURCES})
  target_include_directories(musicoverlay_sdl PRIVATE src)
  target_compile_features(musicoverlay_sdl PRIVATE cxx_std_20)
  target_compile_definitions(musicoverlay_sdl PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
  target_link_libraries(musicoverlay_sdl PRIVATE musicoverlay SDL3::SDL3 user32 gdi32 dwmapi shell32 comdlg32 comctl32)
  if(MSVC)
    target_compile_options(musicoverlay_sdl PRIVATE /EHsc /Zc:__cplusplus)
    set_property(TARGET musicoverlay_sdl PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:DebugDLL>")
  endif()
endif()

if(MUSICOVERLAY_BUILD_INTEGRATION)
  add_library(musicoverlay_integration INTERFACE)
  target_link_libraries(musicoverlay_integration INTERFACE musicoverlay)
  add_library(MusicOverlay::Integration ALIAS musicoverlay_integration)
endif()

if(MUSICOVERLAY_NOCRT)
  install(TARGETS musicoverlay musicoverlay_nocrt_runtime EXPORT MusicOverlayTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
else()
  install(TARGETS musicoverlay EXPORT MusicOverlayTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT MusicOverlayTargets NAMESPACE MusicOverlay:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MusicOverlay)
