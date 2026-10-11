#pragma once
#include <MusicOverlay/Renderer/IRenderer.h>
#include <cstdint>
#include <memory>

namespace mo {
// Renders a FrameView with the shared overlay painter into a CPU-side,
// premultiplied BGRA buffer (byte order B, G, R, A). Any graphics API can
// upload the result as a texture; the SDL frontend uses it with every SDL
// renderer (OpenGL, Vulkan, Direct3D 11/12, the GPU API, and software).
// COM must be initialized on the calling thread.
class SoftwareRenderer {
public:
  SoftwareRenderer();
  ~SoftwareRenderer();
  SoftwareRenderer(const SoftwareRenderer&) = delete;
  SoftwareRenderer& operator=(const SoftwareRenderer&) = delete;

  bool initialize();
  // Renders at frame.width x frame.height, resizing internal surfaces as needed.
  bool render(const FrameView& frame);
  [[nodiscard]] const std::uint8_t* pixels() const noexcept;
  [[nodiscard]] std::uint32_t stride() const noexcept;
  [[nodiscard]] std::uint32_t width() const noexcept;
  [[nodiscard]] std::uint32_t height() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}
