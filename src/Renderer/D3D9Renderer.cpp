#include <cstring>

#include "Renderers.h"
namespace mo
{
namespace
{
struct QuadVertex { float x, y, z, rhw, u, v; };
constexpr DWORD kQuadFvf = D3DFVF_XYZRHW | D3DFVF_TEX1;
}

bool D3D9Renderer::initialize(const NativeTarget& target)
{
  device_ = static_cast<IDirect3DDevice9*>(target.device);
  if (!device_) return false;
  Microsoft::WRL::ComPtr<IDirect3DDevice9Ex> ex;
  deviceEx_ = SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&ex)));
  return software_.initialize();
}

void D3D9Renderer::render(const FrameView& frame)
{
  if (!device_ || !frame.width || !frame.height || !software_.render(frame)) return;
  const UINT width = software_.width(), height = software_.height();
  if (!texture_ || textureWidth_ != width || textureHeight_ != height)
  {
    texture_.Reset();
    // D3D9Ex has no managed pool; a dynamic default-pool texture is released on reset via releaseTargets().
    const HRESULT created = deviceEx_
      ? device_->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture_, nullptr)
      : device_->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture_, nullptr);
    if (FAILED(created)) { texture_.Reset(); return; }
    textureWidth_ = width; textureHeight_ = height;
  }
  D3DLOCKED_RECT locked{};
  if (FAILED(texture_->LockRect(0, &locked, nullptr, deviceEx_ ? D3DLOCK_DISCARD : 0))) return;
  // D3DFMT_A8R8G8B8 is B, G, R, A in memory, matching the painter's premultiplied BGRA output.
  for (UINT y = 0; y < height; ++y)
    std::memcpy(static_cast<BYTE*>(locked.pBits) + static_cast<std::size_t>(y) * static_cast<std::size_t>(locked.Pitch),
                software_.pixels() + static_cast<std::size_t>(y) * software_.stride(), software_.stride());
  texture_->UnlockRect(0);

  if (!stateBlock_) device_->CreateStateBlock(D3DSBT_ALL, &stateBlock_);
  if (stateBlock_) stateBlock_->Capture();
  // Works both inside and outside the host's BeginScene/EndScene pair.
  const bool ownScene = SUCCEEDED(device_->BeginScene());

  device_->SetPixelShader(nullptr);
  device_->SetVertexShader(nullptr);
  device_->SetFVF(kQuadFvf);
  device_->SetTexture(0, texture_.Get());
  device_->SetTexture(1, nullptr);
  device_->SetRenderState(D3DRS_ZENABLE, FALSE);
  device_->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
  device_->SetRenderState(D3DRS_LIGHTING, FALSE);
  device_->SetRenderState(D3DRS_FOGENABLE, FALSE);
  device_->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  device_->SetRenderState(D3DRS_STENCILENABLE, FALSE);
  device_->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
  device_->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
  device_->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
  device_->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
  device_->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
  device_->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
  device_->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
  device_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
  device_->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
  device_->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
  device_->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
  device_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
  device_->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
  device_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
  device_->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
  device_->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
  device_->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
  device_->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
  device_->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
  device_->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
  device_->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
  device_->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
  device_->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
  device_->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
  device_->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);

  // Pre-transformed quad; the half-pixel offset maps texels 1:1 onto pixels.
  const float w = static_cast<float>(width), h = static_cast<float>(height);
  const QuadVertex quad[4] = {
    {-.5f, -.5f, 0.f, 1.f, 0.f, 0.f},
    {w - .5f, -.5f, 0.f, 1.f, 1.f, 0.f},
    {-.5f, h - .5f, 0.f, 1.f, 0.f, 1.f},
    {w - .5f, h - .5f, 0.f, 1.f, 1.f, 1.f},
  };
  device_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(QuadVertex));

  if (ownScene) device_->EndScene();
  if (stateBlock_) stateBlock_->Apply();
}
}
