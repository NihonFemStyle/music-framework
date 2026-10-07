#include "Renderers.h"
namespace mo
{
bool D3D10Renderer::initialize(const NativeTarget& t)
{
  device_=static_cast<ID3D10Device*>(t.device); swap_=static_cast<IDXGISwapChain*>(t.swapChain);
  if (!device_ || !swap_) return false;
  if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_.ReleaseAndGetAddressOf())) || !painter_.initialize()) return false;
  createTarget(); return target_ != nullptr;
  }
  void D3D10Renderer::createTarget()
  {
    target_.Reset(); Microsoft::WRL::ComPtr<IDXGISurface> surface;
    if (FAILED(swap_->GetBuffer(0, IID_PPV_ARGS(&surface)))) return;
    auto props=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
    factory_->CreateDxgiSurfaceRenderTarget(surface.Get(), &props, &target_);
    }
    void D3D10Renderer::resize(std::uint32_t,std::uint32_t){ createTarget(); }
    void D3D10Renderer::render(const FrameView& f){ if(!target_)return; target_->BeginDraw(); painter_.paint(target_.Get(),f); if(target_->EndDraw()==D2DERR_RECREATE_TARGET)target_.Reset(); }
    }
