#pragma once
#include <MusicOverlay/Renderer/IRenderer.h>
#include <MusicOverlay/Renderer/SoftwareRenderer.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <d3d10_1.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <d3d9.h>
#include <dxgi.h>
#include <vector>
#include <wrl/client.h>

#include "UI/OverlayPainter.h"
namespace mo
{
// Direct3D 9: the shared painter renders into a CPU buffer that is uploaded to
// a texture and drawn as a premultiplied-alpha quad. Host render state is saved
// and restored with a state block.
class D3D9Renderer final : public IRenderer
{
  public:
  bool initialize(const NativeTarget&) override;
  void resize(std::uint32_t, std::uint32_t) override { releaseTargets(); }
  void render(const FrameView&) override;
  void releaseTargets() override { texture_.Reset(); stateBlock_.Reset(); textureWidth_ = textureHeight_ = 0; }
  private:
  IDirect3DDevice9* device_{};
  bool deviceEx_{};
  SoftwareRenderer software_;
  Microsoft::WRL::ComPtr<IDirect3DTexture9> texture_;
  Microsoft::WRL::ComPtr<IDirect3DStateBlock9> stateBlock_;
  UINT textureWidth_{}, textureHeight_{};
  };
  class D3D10Renderer final : public IRenderer
  {
    public: bool initialize(const NativeTarget&) override; void resize(std::uint32_t,std::uint32_t) override; void render(const FrameView&) override; void releaseTargets() override { target_.Reset(); }
    private: void createTarget(); ID3D10Device* device_{}; IDXGISwapChain* swap_{}; Microsoft::WRL::ComPtr<ID2D1Factory> factory_; Microsoft::WRL::ComPtr<ID2D1RenderTarget> target_; OverlayPainter painter_;
    };
    class D3D11Renderer final : public IRenderer
    {
      public: bool initialize(const NativeTarget&) override; void resize(std::uint32_t,std::uint32_t) override; void render(const FrameView&) override; void releaseTargets() override { target_.Reset(); }
      private: void createTarget(); ID3D11Device* device_{}; IDXGISwapChain* swap_{}; Microsoft::WRL::ComPtr<ID2D1Factory> factory_; Microsoft::WRL::ComPtr<ID2D1RenderTarget> target_; OverlayPainter painter_;
      };
      // Direct3D 12: renders through D3D11On12 on the host's command queue. The
      // host's own command lists are never touched; see docs/INTEGRATION.md for
      // the state and ordering contract.
      class D3D12Renderer final : public IRenderer
      {
        public:
        ~D3D12Renderer() override;
        bool initialize(const NativeTarget&) override;
        void resize(std::uint32_t, std::uint32_t) override;
        void render(const FrameView&) override;
        void releaseTargets() override;
        private:
        bool createTargets();
        ID3D12Device* device_{}; IDXGISwapChain* swap_{}; ID3D12CommandQueue* queue_{};
        UINT state_{};
        Microsoft::WRL::ComPtr<ID3D11Device> device11_;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context11_;
        Microsoft::WRL::ComPtr<ID3D11On12Device> on12_;
        Microsoft::WRL::ComPtr<ID2D1Factory1> factory_;
        Microsoft::WRL::ComPtr<ID2D1Device> d2dDevice_;
        Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2dContext_;
        std::vector<Microsoft::WRL::ComPtr<ID3D11Resource>> wrapped_;
        std::vector<Microsoft::WRL::ComPtr<ID2D1Bitmap1>> bitmaps_;
        OverlayPainter painter_;
        bool painterReady_{};
        };
        }
