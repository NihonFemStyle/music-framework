#pragma once
#include <MusicOverlay/Renderer/IRenderer.h>
#include <MusicOverlay/Core/OverlayState.h>

namespace mo {
// Host-owned devices/swap chains remain owned by the host. Call render before Present.
class IntegrationContext {
public:
  explicit IntegrationContext(Backend backend) : renderer_(createRenderer(backend)) {}
  bool initialize(const NativeTarget& target) { return renderer_ && renderer_->initialize(target); }
  void resize(std::uint32_t w, std::uint32_t h) { renderer_->resize(w, h); }
  void render(const FrameView& frame) { renderer_->render(frame); }
private:
  std::unique_ptr<IRenderer> renderer_;
};
}
