#include "OverlayPainter.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace mo {
bool OverlayPainter::initialize() {
  if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &write_))) return false;
  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic_)))) return false;
  if (FAILED(write_->CreateTextFormat(L"Segoe UI Variable Display", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
      DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 22.0f, L"", &title_))) return false;
  return SUCCEEDED(write_->CreateTextFormat(L"Segoe UI Variable Text", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
      DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"", &detail_));
}

bool OverlayPainter::decodeBitmap(ID2D1RenderTarget* rt, IWICBitmapSource* source, ID2D1Bitmap** output) {
  Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
  return SUCCEEDED(wic_->CreateFormatConverter(&converter)) &&
    SUCCEEDED(converter->Initialize(source, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)) &&
    SUCCEEDED(rt->CreateBitmapFromWicBitmap(converter.Get(), nullptr, output));
}

void OverlayPainter::drawFittedText(ID2D1RenderTarget* rt, const std::wstring& text, D2D1_RECT_F box,
    float maximum, float minimum, ID2D1Brush* brush, DWRITE_FONT_WEIGHT weight) {
  float width=std::max(1.f,box.right-box.left),height=std::max(1.f,box.bottom-box.top),size=maximum;
  Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
  Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
  write_->CreateTextFormat(L"Segoe UI Variable Display",nullptr,weight,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"",&format);
  write_->CreateTextLayout(text.c_str(),static_cast<UINT32>(text.size()),format.Get(),width,height,&layout);
  DWRITE_TEXT_METRICS metrics{}; if(layout)layout->GetMetrics(&metrics);
  if(metrics.widthIncludingTrailingWhitespace>width||metrics.height>height){float scale=std::min(width/std::max(1.f,metrics.widthIncludingTrailingWhitespace),height/std::max(1.f,metrics.height));size=std::max(minimum,size*scale);format.Reset();layout.Reset();write_->CreateTextFormat(L"Segoe UI Variable Display",nullptr,weight,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"",&format);write_->CreateTextLayout(text.c_str(),static_cast<UINT32>(text.size()),format.Get(),width,height,&layout);}
  if(layout)rt->DrawTextLayout(D2D1::Point2F(box.left,box.top),layout.Get(),brush,D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void OverlayPainter::updateLogo(ID2D1RenderTarget* rt, const Appearance& a) {
  std::wstring requested = a.logoPath ? a.logoPath : L"";
  if (requested == logoPath_ && a.logoData == logoData_) return;
  logoPath_ = requested; logoData_ = a.logoData; logo_.Reset();
  if (a.logoData && a.logoDataSize) {
    Microsoft::WRL::ComPtr<IWICStream> stream;
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    if (SUCCEEDED(wic_->CreateStream(&stream)) &&
        SUCCEEDED(stream->InitializeFromMemory(const_cast<BYTE*>(a.logoData), a.logoDataSize)) &&
        SUCCEEDED(wic_->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder)) &&
        SUCCEEDED(decoder->GetFrame(0, &frame))) decodeBitmap(rt, frame.Get(), logo_.ReleaseAndGetAddressOf());
    return;
  }
  if (requested.empty()) return;
  Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
  Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
  if (SUCCEEDED(wic_->CreateDecoderFromFilename(requested.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder)) &&
      SUCCEEDED(decoder->GetFrame(0, &frame))) decodeBitmap(rt, frame.Get(), logo_.ReleaseAndGetAddressOf());
}

void OverlayPainter::updateArtwork(ID2D1RenderTarget* rt, const TrackInfo& t) {
  if (artworkRevision_ == t.revision) return;
  artworkRevision_ = t.revision; artwork_.Reset();
  if (t.artwork.empty() || t.artwork.size() > UINT_MAX) return;
  Microsoft::WRL::ComPtr<IWICStream> stream;
  Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
  Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
  if (FAILED(wic_->CreateStream(&stream)) ||
      FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(t.artwork.data()), static_cast<DWORD>(t.artwork.size()))) ||
      FAILED(wic_->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder)) ||
      FAILED(decoder->GetFrame(0, &frame))) return;
  decodeBitmap(rt, frame.Get(), artwork_.ReleaseAndGetAddressOf());
  Microsoft::WRL::ComPtr<IWICBitmapScaler> scaler; Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
  if(SUCCEEDED(wic_->CreateBitmapScaler(&scaler))&&SUCCEEDED(scaler->Initialize(frame.Get(),1,1,WICBitmapInterpolationModeFant))&&SUCCEEDED(wic_->CreateFormatConverter(&converter))&&SUCCEEDED(converter->Initialize(scaler.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))){BYTE pixel[4]{};if(SUCCEEDED(converter->CopyPixels(nullptr,4,4,pixel))){float r=pixel[0],g=pixel[1],b=pixel[2],bright=std::max({r,g,b});if(bright<8.f){artworkAccent_=0x68D5C8;}else{float scale=std::clamp(235.f/bright,1.15f,3.25f);auto vivid=[scale](float channel){return static_cast<std::uint32_t>(std::clamp((channel-24.f)*scale+32.f,32.f,255.f));};artworkAccent_=(vivid(r)<<16)|(vivid(g)<<8)|vivid(b);}}}
}

void OverlayPainter::paint(ID2D1RenderTarget* rt, const FrameView& frame) {
  const auto& t = frame.track; const auto& a = frame.appearance;
  float w = static_cast<float>(frame.width), h = static_cast<float>(frame.height);
  updateArtwork(rt, t);
  updateLogo(rt, a);
  std::uint32_t accentColor=a.dynamicAccent?artworkAccent_:a.accentRgb;
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> panel, primary, secondary, accent;
  rt->CreateSolidColorBrush(D2D1::ColorF(0x11151D, a.opacity), &panel);
  rt->CreateSolidColorBrush(D2D1::ColorF(0xF4F7FB, 1.f), &primary);
  rt->CreateSolidColorBrush(D2D1::ColorF(0xAAB4C3, 1.f), &secondary);
  rt->CreateSolidColorBrush(D2D1::ColorF(accentColor, 1.f), &accent);
  auto box = D2D1::RoundedRect(D2D1::RectF(0, 0, w, h), 18, 18);
  if(!a.overkill||a.settingsOpen)rt->FillRoundedRectangle(box, panel.Get());
  if((!a.overkill||a.settingsOpen)&&a.artworkBackground&&artwork_){
    float backgroundHeight=a.settingsOpen?108.f:h;
    Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory;Microsoft::WRL::ComPtr<ID2D1RoundedRectangleGeometry> clipGeometry;Microsoft::WRL::ComPtr<ID2D1Layer> clipLayer;
    rt->GetFactory(d2dFactory.ReleaseAndGetAddressOf());d2dFactory->CreateRoundedRectangleGeometry(D2D1::RoundedRect(D2D1::RectF(0,0,w,backgroundHeight),18,18),clipGeometry.ReleaseAndGetAddressOf());rt->CreateLayer(nullptr,clipLayer.ReleaseAndGetAddressOf());rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),clipGeometry.Get()),clipLayer.Get());
    SYSTEMTIME now{};GetLocalTime(&now);
    if(now.wMonth==4&&now.wDay==1){
      // Intentionally cursed stretch, preserved as an April Fools' Day easter egg.
      rt->DrawBitmap(artwork_.Get(),D2D1::RectF(0,0,w,backgroundHeight),.22f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    }else{
      auto size=artwork_->GetSize();float targetAspect=w/backgroundHeight,sourceAspect=size.width/std::max(1.f,size.height);D2D1_RECT_F source{};
      if(sourceAspect>targetAspect){float croppedWidth=size.height*targetAspect;float left=(size.width-croppedWidth)*.5f;source=D2D1::RectF(left,0,left+croppedWidth,size.height);}else{float croppedHeight=size.width/targetAspect;float top=(size.height-croppedHeight)*.5f;source=D2D1::RectF(0,top,size.width,top+croppedHeight);}
      rt->DrawBitmap(artwork_.Get(),D2D1::RectF(0,0,w,backgroundHeight),.22f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,source);
    }
    rt->PopLayer();
  }
  float textLeft = 22.f;
  if(a.overkill&&!a.settingsOpen){
    float barWidth=w/32.f;for(std::size_t i=0;i<frame.spectrum.size();++i){float level=std::clamp(frame.spectrum[i],0.f,1.f);float barHeight=4.f+level*(h-4.f);Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> spectrumBrush;rt->CreateSolidColorBrush(D2D1::ColorF(accentColor,.22f+.68f*level),&spectrumBrush);float left=float(i)*barWidth;float right=float(i+1)*barWidth-1.f;float top=a.corner<2?0.f:h-barHeight;float bottom=a.corner<2?barHeight:h;rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(left,top,right,bottom),3,3),spectrumBrush.Get());}
    if(artwork_)rt->DrawBitmap(artwork_.Get(),D2D1::RectF(16,14,122,120),1.f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);float overkillLeft=artwork_?140.f:22.f;std::wstring overkillTitle=t.title.empty()?L"Nothing playing":t.title;std::wstring overkillArtist=t.artist.empty()?L"Windows media session":t.artist;drawFittedText(rt,overkillTitle,D2D1::RectF(overkillLeft,18,w-22,78),28,13,primary.Get(),DWRITE_FONT_WEIGHT_SEMI_BOLD);drawFittedText(rt,overkillArtist,D2D1::RectF(overkillLeft,82,w-22,112),16,10,secondary.Get(),DWRITE_FONT_WEIGHT_NORMAL);if(a.showBanner&&logo_)rt->DrawBitmap(logo_.Get(),D2D1::RectF(18,128,168,174),.88f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);return;
  }
  if (artwork_&&!a.artworkBackground) {
    float artBottom = a.compact ? 66.f : 94.f;
    rt->DrawBitmap(artwork_.Get(), D2D1::RectF(12, 10, 12+(artBottom-10), artBottom), 1.f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    textLeft = 18+(artBottom-10);
  }
  std::wstring title = t.title.empty() ? L"Nothing playing" : t.title;
  std::wstring artist = t.artist.empty() ? L"Windows media session" : t.artist;
  drawFittedText(rt,title,D2D1::RectF(textLeft,15,w-158,a.compact?55.f:51.f),22,11,primary.Get(),DWRITE_FONT_WEIGHT_SEMI_BOLD);
  if (!a.compact) drawFittedText(rt,artist,D2D1::RectF(textLeft,53,w-125,79),14,9,secondary.Get(),DWRITE_FONT_WEIGHT_NORMAL);
  if (a.showBanner&&logo_) rt->DrawBitmap(logo_.Get(), D2D1::RectF(w-148, 12, w-18, 56), .85f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
  std::uint64_t now=GetTickCount64();
  if(progressRevision_!=t.revision){
    float sample=t.duration100ns>0?std::clamp(float(double(t.position100ns)/double(t.duration100ns)),0.f,1.f):0.f;
    bool trackChanged=progressTitle_!=t.title;
    bool playbackChanged=progressPlaying_!=t.playing;
    float predicted=progressSample_;
    if(progressSampleTick_&&t.duration100ns>0&&t.playing)predicted+=float(double(now-progressSampleTick_)*10000.0/double(t.duration100ns));
    float seekThreshold=t.duration100ns>0?std::max(.02f,float(50000000.0/double(t.duration100ns))):.02f;
    if(trackChanged||playbackChanged||std::abs(sample-predicted)>seekThreshold){progressSample_=sample;if(trackChanged||playbackChanged)progressSmooth_=sample;}
    else progressSample_=t.playing?predicted:sample;
    progressTitle_=t.title;progressPlaying_=t.playing;progressSampleTick_=now;progressRevision_=t.revision;
  }
  float ratio=progressSample_;if(t.playing&&t.duration100ns>0)ratio+=float(double(now-progressSampleTick_)*10000.0/double(t.duration100ns));ratio=std::clamp(ratio,0.f,1.f);
  float elapsed=progressFrameTick_?float(now-progressFrameTick_)/1000.f:0.f;progressFrameTick_=now;float blend=1.f-std::exp(-24.f*elapsed);progressSmooth_+=std::clamp(blend,0.f,1.f)*(ratio-progressSmooth_);ratio=progressSmooth_;
  float progressY=a.compact?62.f:94.f;
  rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(textLeft,progressY,w-22,progressY+5),3,3),secondary.Get());
  rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(textLeft,progressY,textLeft+(w-textLeft-22)*ratio,progressY+5),3,3),accent.Get());
  if (a.settingsOpen) {
    rt->DrawLine(D2D1::Point2F(18,108),D2D1::Point2F(w-18,108),secondary.Get(),1);
    const wchar_t* heading=L"Overlay settings"; rt->DrawTextW(heading,16,title_.Get(),D2D1::RectF(20,118,w-20,150),primary.Get());
    const wchar_t* opacity=L"Transparency"; rt->DrawTextW(opacity,12,detail_.Get(),D2D1::RectF(20,158,130,180),secondary.Get());
    float sx=140, ex=w-24, sy=168; rt->DrawLine(D2D1::Point2F(sx,sy),D2D1::Point2F(ex,sy),secondary.Get(),5);
    float value=(std::clamp(a.opacity,.1f,1.f)-.1f)/.9f; rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(sx+(ex-sx)*value,sy),7,7),accent.Get());
    const wchar_t* compact=L"Compact mode"; rt->DrawTextW(compact,12,detail_.Get(),D2D1::RectF(20,192,150,216),primary.Get());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(w-70,192,w-24,214),11,11),accent.Get(),2);
    if(a.compact) rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(w-36,203),7,7),accent.Get());
    const wchar_t* palette=L"Accent"; rt->DrawTextW(palette,6,detail_.Get(),D2D1::RectF(20,224,100,246),primary.Get());
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(w-38,234),12,12),accent.Get());
    const wchar_t* dynamic=L"Dynamic accent";rt->DrawTextW(dynamic,14,detail_.Get(),D2D1::RectF(20,257,150,280),primary.Get());rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(w-70,257,w-24,279),11,11),accent.Get(),2);if(a.dynamicAccent)rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(w-36,268),7,7),accent.Get());
    const wchar_t* art=L"Artwork background";rt->DrawTextW(art,18,detail_.Get(),D2D1::RectF(20,291,180,314),primary.Get());rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(w-70,291,w-24,313),11,11),accent.Get(),2);if(a.artworkBackground)rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(w-36,302),7,7),accent.Get());
    const wchar_t* keyLabel=L"Double-tap shortcut";rt->DrawTextW(keyLabel,19,detail_.Get(),D2D1::RectF(20,325,180,348),primary.Get());
    const wchar_t* keyName=a.activationKeyName?a.activationKeyName:L"Shift";rt->DrawTextW(keyName,static_cast<UINT32>(wcslen(keyName)),detail_.Get(),D2D1::RectF(w-160,325,w-22,348),accent.Get());
    const wchar_t* renderer=L"Renderer (restart)";rt->DrawTextW(renderer,18,detail_.Get(),D2D1::RectF(20,359,180,382),primary.Get());const wchar_t* backend=a.backendName?a.backendName:L"DirectX 11";rt->DrawTextW(backend,static_cast<UINT32>(wcslen(backend)),detail_.Get(),D2D1::RectF(w-130,359,w-22,382),accent.Get());
    const wchar_t* overkill=L"Overkill mode";rt->DrawTextW(overkill,13,detail_.Get(),D2D1::RectF(20,393,180,416),primary.Get());rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(w-70,393,w-24,415),11,11),accent.Get(),2);if(a.overkill)rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(w-36,404),7,7),accent.Get());
    const wchar_t* corner=L"Overkill corner";rt->DrawTextW(corner,15,detail_.Get(),D2D1::RectF(20,427,180,450),primary.Get());const wchar_t* cornerName=a.cornerName?a.cornerName:L"Top right";rt->DrawTextW(cornerName,static_cast<UINT32>(wcslen(cornerName)),detail_.Get(),D2D1::RectF(w-140,427,w-22,450),accent.Get());
    const wchar_t* banner=L"Show banner";rt->DrawTextW(banner,11,detail_.Get(),D2D1::RectF(20,461,180,484),primary.Get());rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(w-70,461,w-24,483),11,11),accent.Get(),2);if(a.showBanner)rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(w-36,472),7,7),accent.Get());
  }
}
}
