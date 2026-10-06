#pragma once
#include <MusicOverlay/Core/TrackInfo.h>
#include <cstdint>
#include <memory>

namespace mo {
enum class Backend { D3D9, D3D10, D3D11, D3D12 };
struct NativeTarget { void* window{}; void* device{}; void* swapChain{}; void* commandQueue{}; };
struct Appearance {
  float opacity{0.92f};
  std::uint32_t accentRgb{0x68D5C8};
  bool compact{};
  bool settingsOpen{};
  bool interactive{};
  bool artworkBackground{};
  bool dynamicAccent{};
  const wchar_t* logoPath{};
  const std::uint8_t* logoData{};
  std::uint32_t logoDataSize{};
  const wchar_t* activationKeyName{L"Shift"};
  const wchar_t* backendName{L"DirectX 11"};
};
struct FrameView { std::uint32_t width{}; std::uint32_t height{}; float dpi{96.0f}; TrackInfo track; Appearance appearance; };
class IRenderer {
public:
  virtual ~IRenderer() = default;
  virtual bool initialize(const NativeTarget&) = 0;
  virtual void resize(std::uint32_t, std::uint32_t) = 0;
  virtual void render(const FrameView&) = 0;
};
std::unique_ptr<IRenderer> createRenderer(Backend backend);
}

