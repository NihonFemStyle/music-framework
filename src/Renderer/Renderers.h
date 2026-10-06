#pragma once
#include <MusicOverlay/Renderer/IRenderer.h>
#include <wrl/client.h>
#include <d3d9.h>
#include <d3d10_1.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi.h>
#include <d2d1.h>
#include "UI/OverlayPainter.h"

namespace mo {
class D3D9Renderer final : public IRenderer {
public: bool initialize(const NativeTarget&) override; void resize(std::uint32_t,std::uint32_t) override{}; void render(const FrameView&) override;
private: IDirect3DDevice9* device_{};
};
class D3D10Renderer final : public IRenderer {
public: bool initialize(const NativeTarget&) override; void resize(std::uint32_t,std::uint32_t) override; void render(const FrameView&) override;
private: void createTarget(); ID3D10Device* device_{}; IDXGISwapChain* swap_{}; Microsoft::WRL::ComPtr<ID2D1Factory> factory_; Microsoft::WRL::ComPtr<ID2D1RenderTarget> target_; OverlayPainter painter_;
};
class D3D11Renderer final : public IRenderer {
public: bool initialize(const NativeTarget&) override; void resize(std::uint32_t,std::uint32_t) override; void render(const FrameView&) override;
private: void createTarget(); ID3D11Device* device_{}; IDXGISwapChain* swap_{}; Microsoft::WRL::ComPtr<ID2D1Factory> factory_; Microsoft::WRL::ComPtr<ID2D1RenderTarget> target_; OverlayPainter painter_;
};
class D3D12Renderer final : public IRenderer {
public: bool initialize(const NativeTarget&) override; void resize(std::uint32_t,std::uint32_t) override{}; void render(const FrameView&) override;
private: ID3D12Device* device_{}; IDXGISwapChain* swap_{}; ID3D12CommandQueue* queue_{};
};
}

