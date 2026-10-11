#include "Renderers.h"
#include <dxgi1_4.h>

namespace mo
{
// Contract (see docs/INTEGRATION.md):
//  * render() is called once per frame after the host has submitted its own
//    command lists for the frame to the same queue, and before Present.
//  * The current back buffer is in NativeTarget::resourceState (default
//    D3D12_RESOURCE_STATE_PRESENT) and is returned in that state.
//  * releaseTargets() is called before ResizeBuffers and resize() afterwards.
// The overlay is recorded by D3D11On12 on the host queue, so GPU ordering with
// the host's work follows queue submission order and no host command list,
// descriptor heap, or fence is touched.
D3D12Renderer::~D3D12Renderer()
{
  releaseTargets();
}

bool D3D12Renderer::initialize(const NativeTarget& t)
{
  device_ = static_cast<ID3D12Device*>(t.device);
  swap_ = static_cast<IDXGISwapChain*>(t.swapChain);
  queue_ = static_cast<ID3D12CommandQueue*>(t.commandQueue);
  state_ = t.resourceState;
  if (!device_ || !swap_ || !queue_) return false;

  IUnknown* queues[] = {queue_};
  if (FAILED(D3D11On12CreateDevice(device_, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, queues, 1, 0, &device11_, &context11_, nullptr))) return false;
  if (FAILED(device11_.As(&on12_))) return false;

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi;
  if (FAILED(device11_.As(&dxgi))) return false;
  D2D1_FACTORY_OPTIONS options{};
  if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &options, reinterpret_cast<void**>(factory_.ReleaseAndGetAddressOf())))) return false;
  if (FAILED(factory_->CreateDevice(dxgi.Get(), &d2dDevice_))) return false;
  if (FAILED(d2dDevice_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2dContext_))) return false;
  // One device context draws into every back buffer, so painter resources are shared across them.
  d2dContext_->SetDpi(96.f, 96.f);
  painterReady_ = painter_.initialize();
  return painterReady_ && createTargets();
}

bool D3D12Renderer::createTargets()
{
  DXGI_SWAP_CHAIN_DESC desc{};
  if (FAILED(swap_->GetDesc(&desc)) || !desc.BufferCount) return false;
  wrapped_.assign(desc.BufferCount, nullptr);
  bitmaps_.assign(desc.BufferCount, nullptr);
  const D3D11_RESOURCE_FLAGS flags{D3D11_BIND_RENDER_TARGET, 0, 0, 0};
  const auto state = static_cast<D3D12_RESOURCE_STATES>(state_);
  const auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                                                  D2D1::PixelFormat(desc.BufferDesc.Format, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.f, 96.f);
  for (UINT i = 0; i < desc.BufferCount; ++i)
  {
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
    if (FAILED(swap_->GetBuffer(i, IID_PPV_ARGS(&buffer)))) return false;
    if (FAILED(on12_->CreateWrappedResource(buffer.Get(), &flags, state, state, IID_PPV_ARGS(&wrapped_[i])))) return false;
    Microsoft::WRL::ComPtr<IDXGISurface> surface;
    if (FAILED(wrapped_[i].As(&surface))) return false;
    if (FAILED(d2dContext_->CreateBitmapFromDxgiSurface(surface.Get(), &properties, &bitmaps_[i]))) return false;
  }
  return true;
}

void D3D12Renderer::releaseTargets()
{
  if (d2dContext_) d2dContext_->SetTarget(nullptr);
  bitmaps_.clear();
  wrapped_.clear();
  // D3D11On12 defers destruction of wrapped resources until the next flush;
  // flushing here lets the host resize its swap chain immediately afterwards.
  if (context11_) context11_->Flush();
}

void D3D12Renderer::resize(std::uint32_t, std::uint32_t)
{
  releaseTargets();
  if (on12_ && d2dContext_) createTargets();
}

void D3D12Renderer::render(const FrameView& frame)
{
  if (!painterReady_ || !on12_) return;
  if (wrapped_.empty() && !createTargets()) return;
  Microsoft::WRL::ComPtr<IDXGISwapChain3> swap3;
  UINT index = 0;
  if (SUCCEEDED(swap_->QueryInterface(IID_PPV_ARGS(&swap3)))) index = swap3->GetCurrentBackBufferIndex();
  if (index >= wrapped_.size() || !wrapped_[index] || !bitmaps_[index]) return;

  ID3D11Resource* resource = wrapped_[index].Get();
  on12_->AcquireWrappedResources(&resource, 1);
  d2dContext_->SetTarget(bitmaps_[index].Get());
  d2dContext_->BeginDraw();
  painter_.paint(d2dContext_.Get(), frame);
  const HRESULT drawn = d2dContext_->EndDraw();
  d2dContext_->SetTarget(nullptr);
  on12_->ReleaseWrappedResources(&resource, 1);
  // Submits the recorded overlay to the host queue.
  context11_->Flush();
  if (drawn == D2DERR_RECREATE_TARGET) releaseTargets();
}
}
