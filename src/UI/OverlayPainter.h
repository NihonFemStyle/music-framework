#pragma once
#include <MusicOverlay/Renderer/IRenderer.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
namespace mo
{
class OverlayPainter
{
  public:
  bool initialize();
  void paint(ID2D1RenderTarget* target, const FrameView& frame);
  private:
  void updateArtwork(ID2D1RenderTarget* target, const TrackInfo& track);
  void updateLogo(ID2D1RenderTarget* target, const Appearance& appearance);
  bool decodeBitmap(ID2D1RenderTarget* target, IWICBitmapSource* source, ID2D1Bitmap** output,
  bool forceOpaque = false);
  void drawFittedText(ID2D1RenderTarget* target, const std::wstring& text, D2D1_RECT_F box,
  float maximumSize, float minimumSize, ID2D1Brush* brush, DWRITE_FONT_WEIGHT weight);
  Microsoft::WRL::ComPtr<IDWriteFactory> write_;
  Microsoft::WRL::ComPtr<IWICImagingFactory> wic_;
  Microsoft::WRL::ComPtr<IDWriteTextFormat> title_;
  Microsoft::WRL::ComPtr<IDWriteTextFormat> detail_;
  Microsoft::WRL::ComPtr<ID2D1Bitmap> artwork_;
  Microsoft::WRL::ComPtr<ID2D1Bitmap> opaqueArtwork_;
  Microsoft::WRL::ComPtr<ID2D1Bitmap> logo_;
  std::uint64_t artworkRevision_{};
  std::uint32_t artworkAccent_{0x68D5C8};
  std::uint64_t progressRevision_{};
  std::uint64_t progressSampleTick_{};
  std::uint64_t progressFrameTick_{};
  float progressSample_{};
  float progressSmooth_{};
  bool progressPlaying_{};
  std::wstring progressTitle_;
  std::wstring logoPath_;
  const std::uint8_t* logoData_{};
  };
  }
