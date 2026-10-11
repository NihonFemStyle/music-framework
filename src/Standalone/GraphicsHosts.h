#pragma once
#include <MusicOverlay/Renderer/IRenderer.h>
#include <memory>
#include <windows.h>

namespace mo::standalone {
// Owns the device, swap chain, and presentation path for one Direct3D version
// and drives the matching integration renderer, so the standalone overlay runs
// on the backend selected in settings.
//  * Direct3D 10, 11, 12: premultiplied-alpha composition swap chain shown
//    through DirectComposition (true transparency, including OBS capture).
//  * Direct3D 9: Direct3D 9Ex with an ARGB back buffer presented into the
//    DWM-extended frame of the layered overlay window.
class GraphicsHost {
public:
  virtual ~GraphicsHost() = default;
  virtual bool create(HWND hwnd) = 0;
  virtual void resize(UINT width, UINT height) = 0;
  virtual void render(const FrameView& frame) = 0;
  virtual void setOpacity(float opacity) = 0;
  [[nodiscard]] virtual int backend() const = 0;
};

// Direct3D 9 presents through the window's redirection surface, so its window
// must be created without WS_EX_NOREDIRECTIONBITMAP.
[[nodiscard]] bool backendNeedsRedirectionSurface(int backend);
// Creates the requested backend (9, 10, 11, or 12). Falls back to Direct3D 11
// when the requested backend cannot be created; returns null only if that fails too.
std::unique_ptr<GraphicsHost> createGraphicsHost(int backend, HWND hwnd);
}
