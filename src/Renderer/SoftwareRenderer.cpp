#include <MusicOverlay/Renderer/SoftwareRenderer.h>

#include <d2d1.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cstring>
#include <vector>

#include "UI/OverlayPainter.h"

namespace mo
{
using Microsoft::WRL::ComPtr;

struct SoftwareRenderer::Impl
{
  ComPtr<ID2D1Factory> factory;
  ComPtr<IWICImagingFactory> wic;
  ComPtr<IWICBitmap> bitmap;
  ComPtr<ID2D1RenderTarget> target;
  std::unique_ptr<OverlayPainter> painter;
  std::vector<std::uint8_t> pixels;
  std::uint32_t width{}, height{}, stride{};

  bool createSurface(std::uint32_t w, std::uint32_t h)
  {
    target.Reset(); bitmap.Reset(); painter.reset();
    width = height = stride = 0;
    if (FAILED(wic->CreateBitmap(w, h, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bitmap))) return false;
    // Fixed 96 DPI: the painter's layout is expressed in physical pixels.
    const auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.f, 96.f);
    if (FAILED(factory->CreateWicBitmapRenderTarget(bitmap.Get(), properties, &target))) return false;
    // Painter bitmaps belong to one render target, so a new target needs a new painter.
    painter = std::make_unique<OverlayPainter>();
    if (!painter->initialize()) { painter.reset(); return false; }
    width = w; height = h; stride = w * 4;
    pixels.assign(static_cast<std::size_t>(stride) * h, 0);
    return true;
  }
};

SoftwareRenderer::SoftwareRenderer() : impl_(std::make_unique<Impl>()) {}
SoftwareRenderer::~SoftwareRenderer() = default;

bool SoftwareRenderer::initialize()
{
  if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, impl_->factory.ReleaseAndGetAddressOf()))) return false;
  return SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&impl_->wic)));
}

bool SoftwareRenderer::render(const FrameView& frame)
{
  auto& d = *impl_;
  if (!d.factory || !d.wic || !frame.width || !frame.height) return false;
  if ((frame.width != d.width || frame.height != d.height || !d.target) && !d.createSurface(frame.width, frame.height)) return false;
  d.target->BeginDraw();
  d.target->Clear(D2D1::ColorF(0.f, 0.f, 0.f, 0.f));
  d.painter->paint(d.target.Get(), frame);
  if (FAILED(d.target->EndDraw())) { d.target.Reset(); return false; }
  WICRect rect{0, 0, static_cast<INT>(d.width), static_cast<INT>(d.height)};
  ComPtr<IWICBitmapLock> lock;
  if (FAILED(d.bitmap->Lock(&rect, WICBitmapLockRead, &lock))) return false;
  UINT size{}, sourceStride{};
  BYTE* source{};
  if (FAILED(lock->GetStride(&sourceStride)) || FAILED(lock->GetDataPointer(&size, &source)) || !source) return false;
  for (std::uint32_t y = 0; y < d.height; ++y)
    std::memcpy(d.pixels.data() + static_cast<std::size_t>(y) * d.stride, source + static_cast<std::size_t>(y) * sourceStride, d.stride);
  return true;
}

const std::uint8_t* SoftwareRenderer::pixels() const noexcept { return impl_->pixels.data(); }
std::uint32_t SoftwareRenderer::stride() const noexcept { return impl_->stride; }
std::uint32_t SoftwareRenderer::width() const noexcept { return impl_->width; }
std::uint32_t SoftwareRenderer::height() const noexcept { return impl_->height; }
}
