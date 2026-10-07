#include "Renderers.h"
namespace mo
{
bool D3D9Renderer::initialize(const NativeTarget& t) { device_ = static_cast<IDirect3DDevice9*>(t.device); return device_ != nullptr; }
void D3D9Renderer::render(const FrameView& frame)
{
  if (!device_) return;
  IDirect3DSurface9* surface{};
  if (FAILED(device_->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &surface))) return;
  HDC dc{};
  if (SUCCEEDED(surface->GetDC(&dc)))
  {
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(244,247,251));
    RECT r{22,18,static_cast<LONG>(frame.width-20),60};
    auto text = frame.track.title.empty() ? L"Nothing playing" : frame.track.title.c_str();
    DrawTextW(dc, text, -1, &r, DT_SINGLELINE|DT_END_ELLIPSIS); surface->ReleaseDC(dc);
    }
    surface->Release();
    }
    }
