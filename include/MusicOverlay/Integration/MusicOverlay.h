#pragma once
#include <MusicOverlay/Renderer/IRenderer.h>
#include <MusicOverlay/Core/OverlayState.h>

namespace mo {
// Host-owned devices/swap chains remain owned by the host. Call render before Present.
class IntegrationContext {
public:
  explicit IntegrationContext(Backend backend) : renderer_(createRenderer(backend)) {}
  bool initialize(const NativeTarget& target) { return renderer_ && renderer_->initialize(target); }
  // Call releaseTargets() before ResizeBuffers/Reset, then resize() afterwards.
  void releaseTargets() { if (renderer_) renderer_->releaseTargets(); }
  void resize(std::uint32_t w, std::uint32_t h) { if (renderer_) renderer_->resize(w, h); }
  void render(const FrameView& frame) { if (renderer_) renderer_->render(frame); }
private:
  std::unique_ptr<IRenderer> renderer_;
};
}
