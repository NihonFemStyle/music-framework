#pragma once
#include <MusicOverlay/Core/TrackInfo.h>
#include <cstdint>
#include <array>
#include <memory>

namespace mo {
enum class Backend { D3D9, D3D10, D3D11, D3D12 };
struct NativeTarget {
  void* window{};
  void* device{};       // IDirect3DDevice9*, ID3D10Device*, ID3D11Device*, or ID3D12Device*
  void* swapChain{};    // IDXGISwapChain* (D3D10/11/12)
  void* commandQueue{}; // ID3D12CommandQueue* (D3D12 only)
  // D3D12 only: the D3D12_RESOURCE_STATES value the host leaves the current back
  // buffer in when it calls render(), and expects it to be returned in.
  // The default 0 is D3D12_RESOURCE_STATE_PRESENT.
  std::uint32_t resourceState{};
};
struct Appearance {
  float opacity{0.92f};
  std::uint32_t accentRgb{0x68D5C8};
  bool compact{};
  bool settingsOpen{};
  bool interactive{};
  bool artworkBackground{};
  bool dynamicAccent{};
  bool overkill{};
  bool showBanner{true};
  bool ignoreParenthetical{};
  bool spectrumChroma{};
  bool spectrumCurves{};
  bool artworkSpectrum{};
  std::uint32_t spectrumBars{32};
  std::uint32_t spectrumDelayMs{};
  std::uint32_t spectrumResponse{1};
  std::uint32_t spectrumMaxHeight{100};
  std::uint32_t spectrumFade{10};
  std::uint32_t spectrumHueSpeed{30};
  std::uint32_t corner{};
  const wchar_t* logoPath{};
  const std::uint8_t* logoData{};
  std::uint32_t logoDataSize{};
  const wchar_t* activationKeyName{L"Shift"};
  const wchar_t* fontFamily{L"Segoe UI Variable Display"};
  const wchar_t* backendName{L"DirectX 11"};
  const wchar_t* cornerName{L"Top right"};
};
struct FrameView { std::uint32_t width{}; std::uint32_t height{}; float dpi{96.0f}; TrackInfo track; Appearance appearance; std::array<float,64> spectrum{}; };
class IRenderer {
public:
  virtual ~IRenderer() = default;
  virtual bool initialize(const NativeTarget&) = 0;
  // Call after the host has resized its swap chain (or reset its D3D9 device).
  virtual void resize(std::uint32_t, std::uint32_t) = 0;
  virtual void render(const FrameView&) = 0;
  // Releases every reference to swap-chain buffers or device-pool resources.
  // Call before IDXGISwapChain::ResizeBuffers or IDirect3DDevice9::Reset.
  virtual void releaseTargets() {}
};
std::unique_ptr<IRenderer> createRenderer(Backend backend);
}
