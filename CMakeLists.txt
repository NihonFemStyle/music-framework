cmake_minimum_required(VERSION 3.24)
project(NihonsMusicFramework VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

if(NOT WIN32)
  message(FATAL_ERROR "Nihon's Music Framework is a native Windows project")
endif()

option(MUSICOVERLAY_BUILD_STANDALONE "Build the desktop overlay" ON)
option(MUSICOVERLAY_BUILD_INTEGRATION "Build the reusable integration library" ON)
option(MUSICOVERLAY_NOCRT "Build the constrained no-CRT D3D11 overlay" OFF)

if(MUSICOVERLAY_NOCRT AND NOT MSVC)
  message(FATAL_ERROR "MUSICOVERLAY_NOCRT currently supports MSVC only")
endif()

# MSVC's default Debug flags inject /RTC1 hooks from the CRT. There is no
# supported negative /RTC switch, so remove the option before creating the
# deliberately runtime-free target.
if(MUSICOVERLAY_NOCRT AND MSVC)
  string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
endif()

set(MO_PUBLIC_HEADERS
  include/MusicOverlay/Core/TrackInfo.h
  include/MusicOverlay/Core/OverlayState.h
  include/MusicOverlay/Renderer/IRenderer.h
  include/MusicOverlay/Integration/MusicOverlay.h)

if(NOT MUSICOVERLAY_NOCRT OR MUSICOVERLAY_BUILD_INTEGRATION)
add_library(musicoverlay STATIC
  ${MO_PUBLIC_HEADERS}
  src/Core/OverlayState.cpp
  src/Platform/Windows/MediaSessionManager.cpp
  src/Platform/Windows/MediaSessionManager.h
  src/Renderer/RendererFactory.cpp
  src/Renderer/D3D9Renderer.cpp
  src/Renderer/D3D10Renderer.cpp
  src/Renderer/D3D11Renderer.cpp
  src/Renderer/D3D12Renderer.cpp
  src/Renderer/Renderers.h
  src/UI/OverlayPainter.cpp
  src/UI/OverlayPainter.h)

target_include_directories(musicoverlay
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    src)
target_compile_features(musicoverlay PUBLIC cxx_std_20)
target_compile_definitions(musicoverlay PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(musicoverlay PUBLIC d3d9 d3d10 d3d11 d3d12 dxgi d2d1 dwrite windowscodecs runtimeobject)

if(MSVC)
  target_compile_options(musicoverlay PRIVATE /W4 /permissive- /EHsc /Zc:__cplusplus)
  set_property(TARGET musicoverlay PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:DebugDLL>")
endif()
endif()

if(MUSICOVERLAY_BUILD_STANDALONE)
  if(MUSICOVERLAY_NOCRT)
    add_executable(musicoverlay_app WIN32
      src/NoCRT/NoCrtMain.cpp
      src/Standalone/MusicOverlay.rc
      src/Standalone/resource.h)
    target_compile_definitions(musicoverlay_app PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE MUSICOVERLAY_NOCRT=1)
    target_compile_options(musicoverlay_app PRIVATE /GS- /GR- /EHs-c- /Zl /Oi-)
    target_link_options(musicoverlay_app PRIVATE /NODEFAULTLIB /ENTRY:WinMainCRTStartup)
    target_link_libraries(musicoverlay_app PRIVATE kernel32 user32 gdi32 d3d11 dxgi)
  else()
    add_executable(musicoverlay_app WIN32
      src/Standalone/WinMain.cpp
      src/Standalone/MusicOverlay.rc
      src/Standalone/app.manifest
      src/Standalone/resource.h)
    target_include_directories(musicoverlay_app PRIVATE src)
    target_compile_definitions(musicoverlay_app PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(musicoverlay_app PRIVATE musicoverlay user32 dwmapi shell32 comdlg32 comctl32)
    if(MSVC)
      set_property(TARGET musicoverlay_app PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:DebugDLL>")
    endif()
  endif()
endif()

if(MUSICOVERLAY_BUILD_INTEGRATION)
  add_library(musicoverlay_integration INTERFACE)
  target_link_libraries(musicoverlay_integration INTERFACE musicoverlay)
  add_library(MusicOverlay::Integration ALIAS musicoverlay_integration)
endif()

if(TARGET musicoverlay)
install(TARGETS musicoverlay EXPORT MusicOverlayTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT MusicOverlayTargets NAMESPACE MusicOverlay:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MusicOverlay)
endif()

