#include "GraphicsHosts.h"

#include <MusicOverlay/Integration/MusicOverlay.h>
#include <algorithm>
#include <d3d10_1.h>
#include <d3d11.h>
#include <d3d12.h>
#include <d3d9.h>
#include <dcomp.h>
#include <dxgi1_4.h>
#include <vector>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace mo::standalone {
namespace {
SIZE clientSize(HWND hwnd)
{
  RECT client{};
  GetClientRect(hwnd, &client);
  return SIZE{std::max<LONG>(1, client.right), std::max<LONG>(1, client.bottom)};
}

DXGI_SWAP_CHAIN_DESC1 compositionDesc(SIZE size)
{
  DXGI_SWAP_CHAIN_DESC1 desc{};
  desc.Width = static_cast<UINT>(size.cx);
  desc.Height = static_cast<UINT>(size.cy);
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = 2;
  desc.Scaling = DXGI_SCALING_STRETCH;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
  return desc;
}

// DirectComposition tree shared by the DXGI hosts: one visual whose content is
// the swap chain, with an effect group carrying the overall opacity.
struct Composition
{
  ComPtr<IDCompositionDevice> device;
  ComPtr<IDCompositionTarget> target;
  ComPtr<IDCompositionVisual> visual;
  ComPtr<IDCompositionEffectGroup> effects;

  bool create(HWND hwnd, IUnknown* swapChain, IDXGIDevice* renderingDevice)
  {
    // A rendering device is only needed for DirectComposition surfaces; the
    // D3D10 and D3D12 hosts present swap chains only and pass none.
    const HRESULT created = renderingDevice ? DCompositionCreateDevice(renderingDevice, IID_PPV_ARGS(&device))
                                            : DCompositionCreateDevice2(nullptr, IID_PPV_ARGS(&device));
    return SUCCEEDED(created) && SUCCEEDED(device->CreateTargetForHwnd(hwnd, TRUE, &target)) && SUCCEEDED(device->CreateVisual(&visual)) &&
           SUCCEEDED(device->CreateEffectGroup(&effects)) && SUCCEEDED(visual->SetEffect(effects.Get())) && SUCCEEDED(visual->SetContent(swapChain)) &&
           SUCCEEDED(target->SetRoot(visual.Get())) && SUCCEEDED(device->Commit());
  }
  void setOpacity(float opacity)
  {
    if (!effects || !device) return;
    effects->SetOpacity(std::clamp(opacity, .1f, 1.f));
    device->Commit();
  }
};

// ---------------------------------------------------------------------------
// Direct3D 11 (reference host)
// ---------------------------------------------------------------------------
class D3D11Host final : public GraphicsHost
{
public:
  bool create(HWND hwnd) override
  {
    hwnd_ = hwnd;
    const D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 3, D3D11_SDK_VERSION, &device_, nullptr, &context_))) return false;
    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory2> factory;
    if (FAILED(device_.As(&dxgiDevice)) || FAILED(dxgiDevice->GetAdapter(&adapter)) || FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) return false;
    const auto desc = compositionDesc(clientSize(hwnd));
    ComPtr<IDXGISwapChain1> swap;
    if (FAILED(factory->CreateSwapChainForComposition(device_.Get(), &desc, nullptr, &swap)) || FAILED(swap.As(&swap_))) return false;
    if (!composition_.create(hwnd, swap_.Get(), dxgiDevice.Get())) return false;
    return createOverlay();
  }
  void resize(UINT width, UINT height) override
  {
    if (!swap_ || !width || !height) return;
    context_->ClearState();
    overlay_.reset();
    if (SUCCEEDED(swap_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))) createOverlay();
  }
  void render(const FrameView& frame) override
  {
    if (!overlay_) return;
    overlay_->render(frame);
    swap_->Present(1, 0);
  }
  void setOpacity(float opacity) override { composition_.setOpacity(opacity); }
  int backend() const override { return 11; }
private:
  bool createOverlay()
  {
    overlay_ = std::make_unique<IntegrationContext>(Backend::D3D11);
    return overlay_->initialize({hwnd_, device_.Get(), swap_.Get(), nullptr});
  }
  HWND hwnd_{};
  ComPtr<ID3D11Device> device_;
  ComPtr<ID3D11DeviceContext> context_;
  ComPtr<IDXGISwapChain> swap_;
  Composition composition_;
  std::unique_ptr<IntegrationContext> overlay_;
};

// ---------------------------------------------------------------------------
// Direct3D 10.1
// ---------------------------------------------------------------------------
class D3D10Host final : public GraphicsHost
{
public:
  bool create(HWND hwnd) override
  {
    hwnd_ = hwnd;
    const D3D10_FEATURE_LEVEL1 levels[]{D3D10_FEATURE_LEVEL_10_1, D3D10_FEATURE_LEVEL_10_0};
    for (auto level : levels)
      if (SUCCEEDED(D3D10CreateDevice1(nullptr, D3D10_DRIVER_TYPE_HARDWARE, nullptr, D3D10_CREATE_DEVICE_BGRA_SUPPORT, level, D3D10_1_SDK_VERSION, &device_))) break;
    if (!device_) return false;
    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory2> factory;
    if (FAILED(device_.As(&dxgiDevice)) || FAILED(dxgiDevice->GetAdapter(&adapter)) || FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) return false;
    const auto desc = compositionDesc(clientSize(hwnd));
    ComPtr<IDXGISwapChain1> swap;
    if (FAILED(factory->CreateSwapChainForComposition(device_.Get(), &desc, nullptr, &swap)) || FAILED(swap.As(&swap_))) return false;
    if (!composition_.create(hwnd, swap_.Get(), nullptr)) return false;
    return createViews();
  }
  void resize(UINT width, UINT height) override
  {
    if (!swap_ || !width || !height) return;
    if (overlay_) overlay_->releaseTargets();
    overlay_.reset();
    view_.Reset();
    device_->ClearState();
    if (SUCCEEDED(swap_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))) createViews();
  }
  void render(const FrameView& frame) override
  {
    if (!overlay_ || !view_) return;
    // Flip-model buffers are undefined after Present; start each frame fully transparent.
    const float transparent[4]{0.f, 0.f, 0.f, 0.f};
    device_->ClearRenderTargetView(view_.Get(), transparent);
    overlay_->render(frame);
    swap_->Present(1, 0);
  }
  void setOpacity(float opacity) override { composition_.setOpacity(opacity); }
  int backend() const override { return 10; }
private:
  bool createViews()
  {
    ComPtr<ID3D10Texture2D> buffer;
    if (FAILED(swap_->GetBuffer(0, IID_PPV_ARGS(&buffer))) || FAILED(device_->CreateRenderTargetView(buffer.Get(), nullptr, &view_))) return false;
    overlay_ = std::make_unique<IntegrationContext>(Backend::D3D10);
    return overlay_->initialize({hwnd_, device_.Get(), swap_.Get(), nullptr});
  }
  HWND hwnd_{};
  ComPtr<ID3D10Device1> device_;
  ComPtr<IDXGISwapChain> swap_;
  ComPtr<ID3D10RenderTargetView> view_;
  Composition composition_;
  std::unique_ptr<IntegrationContext> overlay_;
};

// ---------------------------------------------------------------------------
// Direct3D 12 (exercises the D3D12 integration contract)
// ---------------------------------------------------------------------------
class D3D12Host final : public GraphicsHost
{
public:
  ~D3D12Host() override
  {
    waitForGpu();
    overlay_.reset();
    if (fenceEvent_) CloseHandle(fenceEvent_);
  }
  bool create(HWND hwnd) override
  {
    hwnd_ = hwnd;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)))) return false;
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue_)))) return false;
    ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) return false;
    const auto desc = compositionDesc(clientSize(hwnd));
    ComPtr<IDXGISwapChain1> swap;
    if (FAILED(factory->CreateSwapChainForComposition(queue_.Get(), &desc, nullptr, &swap)) || FAILED(swap.As(&swap_))) return false;
    if (!composition_.create(hwnd, swap_.Get(), nullptr)) return false;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors = kBufferCount;
    if (FAILED(device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap_)))) return false;
    rtvStride_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator_)))) return false;
    if (FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(), nullptr, IID_PPV_ARGS(&list_)))) return false;
    list_->Close();
    if (FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)))) return false;
    fenceEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent_ || !createBuffers()) return false;
    overlay_ = std::make_unique<IntegrationContext>(Backend::D3D12);
    // The host leaves back buffers in D3D12_RESOURCE_STATE_PRESENT (resourceState 0) when it calls render().
    return overlay_->initialize({hwnd_, device_.Get(), swap_.Get(), queue_.Get(), 0});
  }
  void resize(UINT width, UINT height) override
  {
    if (!swap_ || !width || !height) return;
    waitForGpu();
    if (overlay_) overlay_->releaseTargets();
    buffers_.clear();
    if (SUCCEEDED(swap_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0)) && createBuffers() && overlay_) overlay_->resize(width, height);
  }
  void render(const FrameView& frame) override
  {
    if (!overlay_ || buffers_.empty()) return;
    const UINT index = swap_->GetCurrentBackBufferIndex();
    if (index >= buffers_.size()) return;
    // Host frame: clear the back buffer to transparent and return it to PRESENT.
    allocator_->Reset();
    list_->Reset(allocator_.Get(), nullptr);
    transition(index, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const float transparent[4]{0.f, 0.f, 0.f, 0.f};
    list_->ClearRenderTargetView(rtv(index), transparent, 0, nullptr);
    transition(index, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    list_->Close();
    ID3D12CommandList* lists[]{list_.Get()};
    queue_->ExecuteCommandLists(1, lists);
    // Overlay: recorded by the renderer on the same queue after the host's work.
    overlay_->render(frame);
    swap_->Present(1, 0);
    waitForGpu();
  }
  void setOpacity(float opacity) override { composition_.setOpacity(opacity); }
  int backend() const override { return 12; }
private:
  static constexpr UINT kBufferCount = 2;
  D3D12_CPU_DESCRIPTOR_HANDLE rtv(UINT index) const
  {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * rtvStride_;
    return handle;
  }
  bool createBuffers()
  {
    buffers_.resize(kBufferCount);
    for (UINT i = 0; i < kBufferCount; ++i)
    {
      if (FAILED(swap_->GetBuffer(i, IID_PPV_ARGS(&buffers_[i])))) return false;
      device_->CreateRenderTargetView(buffers_[i].Get(), nullptr, rtv(i));
    }
    return true;
  }
  void transition(UINT index, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
  {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = buffers_[index].Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    list_->ResourceBarrier(1, &barrier);
  }
  void waitForGpu()
  {
    if (!queue_ || !fence_ || !fenceEvent_) return;
    const UINT64 value = ++fenceValue_;
    if (FAILED(queue_->Signal(fence_.Get(), value))) return;
    if (fence_->GetCompletedValue() < value && SUCCEEDED(fence_->SetEventOnCompletion(value, fenceEvent_))) WaitForSingleObject(fenceEvent_, 5000);
  }
  HWND hwnd_{};
  ComPtr<ID3D12Device> device_;
  ComPtr<ID3D12CommandQueue> queue_;
  ComPtr<IDXGISwapChain3> swap_;
  ComPtr<ID3D12DescriptorHeap> rtvHeap_;
  UINT rtvStride_{};
  std::vector<ComPtr<ID3D12Resource>> buffers_;
  ComPtr<ID3D12CommandAllocator> allocator_;
  ComPtr<ID3D12GraphicsCommandList> list_;
  ComPtr<ID3D12Fence> fence_;
  HANDLE fenceEvent_{};
  UINT64 fenceValue_{};
  Composition composition_;
  std::unique_ptr<IntegrationContext> overlay_; // declared last: released before the device and queue
};

// ---------------------------------------------------------------------------
// Direct3D 9Ex
// ---------------------------------------------------------------------------
class D3D9Host final : public GraphicsHost
{
public:
  ~D3D9Host() override { overlay_.reset(); }
  bool create(HWND hwnd) override
  {
    hwnd_ = hwnd;
    if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d_))) return false;
    const SIZE size = clientSize(hwnd);
    params_.Windowed = TRUE;
    params_.SwapEffect = D3DSWAPEFFECT_DISCARD;
    params_.BackBufferFormat = D3DFMT_A8R8G8B8; // alpha is honored inside the DWM-extended frame
    params_.BackBufferWidth = static_cast<UINT>(size.cx);
    params_.BackBufferHeight = static_cast<UINT>(size.cy);
    params_.BackBufferCount = 1;
    params_.hDeviceWindow = hwnd;
    params_.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    const DWORD flags = D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE;
    if (FAILED(d3d_->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, flags | D3DCREATE_HARDWARE_VERTEXPROCESSING, &params_, nullptr, &device_)) &&
        FAILED(d3d_->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, flags | D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params_, nullptr, &device_)))
      return false;
    overlay_ = std::make_unique<IntegrationContext>(Backend::D3D9);
    return overlay_->initialize({hwnd_, device_.Get(), nullptr, nullptr});
  }
  void resize(UINT width, UINT height) override
  {
    if (!device_ || !width || !height) return;
    if (overlay_) overlay_->releaseTargets();
    params_.BackBufferWidth = width;
    params_.BackBufferHeight = height;
    if (SUCCEEDED(device_->ResetEx(&params_, nullptr)) && overlay_) overlay_->resize(width, height);
  }
  void render(const FrameView& frame) override
  {
    if (!device_ || !overlay_) return;
    device_->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.f, 0);
    if (SUCCEEDED(device_->BeginScene()))
    {
      overlay_->render(frame);
      device_->EndScene();
    }
    const HRESULT presented = device_->PresentEx(nullptr, nullptr, nullptr, nullptr, 0);
    if (presented == D3DERR_DEVICELOST || presented == D3DERR_DEVICEHUNG || presented == D3DERR_DEVICEREMOVED)
    {
      RECT client{};
      GetClientRect(hwnd_, &client);
      resize(static_cast<UINT>(std::max<LONG>(1, client.right)), static_cast<UINT>(std::max<LONG>(1, client.bottom)));
    }
  }
  void setOpacity(float opacity) override
  {
    // The D3D9 overlay is composited as a layered window, so opacity is the layer alpha.
    if (GetWindowLongPtrW(hwnd_, GWL_EXSTYLE) & WS_EX_LAYERED)
      SetLayeredWindowAttributes(hwnd_, 0, static_cast<BYTE>(std::clamp(opacity, .1f, 1.f) * 255.f + .5f), LWA_ALPHA);
  }
  int backend() const override { return 9; }
private:
  HWND hwnd_{};
  ComPtr<IDirect3D9Ex> d3d_;
  ComPtr<IDirect3DDevice9Ex> device_;
  D3DPRESENT_PARAMETERS params_{};
  std::unique_ptr<IntegrationContext> overlay_;
};

std::unique_ptr<GraphicsHost> make(int backend)
{
  switch (backend)
  {
    case 9: return std::make_unique<D3D9Host>();
    case 10: return std::make_unique<D3D10Host>();
    case 12: return std::make_unique<D3D12Host>();
    default: return std::make_unique<D3D11Host>();
  }
}
}

bool backendNeedsRedirectionSurface(int backend) { return backend == 9; }

std::unique_ptr<GraphicsHost> createGraphicsHost(int backend, HWND hwnd)
{
  auto host = make(backend);
  if (host->create(hwnd)) return host;
  if (backend == 11) return nullptr;
  host = make(11);
  return host->create(hwnd) ? std::move(host) : nullptr;
}
}
