#include <algorithm>
#include <cmath>
#include <vector>

#include "Core/MetadataFormatter.h"
#include "OverlayPainter.h"
namespace mo
{
bool OverlayPainter::initialize()
{
  if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &write_))) return false;
  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic_)))) return false;
  if (FAILED(write_->CreateTextFormat(L"Segoe UI Variable Display", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
  DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 22.0f, L"", &title_))) return false;
  return SUCCEEDED(write_->CreateTextFormat(L"Segoe UI Variable Text", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
  DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"", &detail_));
  }

  bool OverlayPainter::decodeBitmap(ID2D1RenderTarget* rt, IWICBitmapSource* source, ID2D1Bitmap** output,
  bool forceOpaque)
  {
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if (FAILED(wic_->CreateFormatConverter(&converter))) return false;
    const auto format = forceOpaque ? GUID_WICPixelFormat32bppBGRA : GUID_WICPixelFormat32bppPBGRA;
    if (FAILED(converter->Initialize(source, format, WICBitmapDitherTypeNone, nullptr, 0,
    WICBitmapPaletteTypeCustom))) return false;
    if (!forceOpaque) return SUCCEEDED(rt->CreateBitmapFromWicBitmap(converter.Get(), nullptr, output));

    UINT width{}, height{};
    if (FAILED(converter->GetSize(&width, &height)) || !width || !height || width > UINT_MAX / 4) return false;
    const UINT stride = width * 4;
    if (height > SIZE_MAX / stride) return false;
    std::vector<BYTE> pixels(static_cast<std::size_t>(stride) * height);
    if (pixels.size() > UINT_MAX) return false;
    if (FAILED(converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()))) return false;
    for (std::size_t index = 3; index < pixels.size(); index += 4)
    {
      pixels[index] = 0xff;
      if(!pixels[index-3]&&!pixels[index-2]&&!pixels[index-1])pixels[index-3]=pixels[index-2]=pixels[index-1]=1;
      }
    const auto properties = D2D1::BitmapProperties(
    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    return SUCCEEDED(rt->CreateBitmap(D2D1::SizeU(width, height), pixels.data(), stride, properties, output));
    }

    void OverlayPainter::drawFittedText(ID2D1RenderTarget* rt, const std::wstring& text, D2D1_RECT_F box,
    float maximum, float minimum, ID2D1Brush* brush, DWRITE_FONT_WEIGHT weight)
    {
      float width=std::max(1.f,box.right-box.left),height=std::max(1.f,box.bottom-box.top),size=maximum;
      Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
      Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
      write_->CreateTextFormat(L"Segoe UI Variable Display",nullptr,weight,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"",&format);
      write_->CreateTextLayout(text.c_str(),static_cast<UINT32>(text.size()),format.Get(),width,height,&layout);
      DWRITE_TEXT_METRICS metrics{}; if(layout)layout->GetMetrics(&metrics);
      if(metrics.widthIncludingTrailingWhitespace>width||metrics.height>height){float scale=std::min(width/std::max(1.f,metrics.widthIncludingTrailingWhitespace),height/std::max(1.f,metrics.height));size=std::max(minimum,size*scale);format.Reset();layout.Reset();write_->CreateTextFormat(L"Segoe UI Variable Display",nullptr,weight,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"",&format);write_->CreateTextLayout(text.c_str(),static_cast<UINT32>(text.size()),format.Get(),width,height,&layout);}
      if(layout)rt->DrawTextLayout(D2D1::Point2F(box.left,box.top),layout.Get(),brush,D2D1_DRAW_TEXT_OPTIONS_CLIP);
      }

      void OverlayPainter::updateLogo(ID2D1RenderTarget* rt, const Appearance& a)
      {
        std::wstring requested = a.logoPath ? a.logoPath : L"";
        if (requested == logoPath_ && a.logoData == logoData_) return;
        logoPath_ = requested; logoData_ = a.logoData; logo_.Reset();
        if (a.logoData && a.logoDataSize)
        {
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

          void OverlayPainter::updateArtwork(ID2D1RenderTarget* rt, const TrackInfo& t)
          {
            if (artworkRevision_ == t.revision) return;
            artworkRevision_ = t.revision; artwork_.Reset(); opaqueArtwork_.Reset();
            if (t.artwork.empty() || t.artwork.size() > UINT_MAX) return;
            Microsoft::WRL::ComPtr<IWICStream> stream;
            Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
            Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
            if (FAILED(wic_->CreateStream(&stream)) ||
            FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(t.artwork.data()), static_cast<DWORD>(t.artwork.size()))) ||
            FAILED(wic_->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder)) ||
            FAILED(decoder->GetFrame(0, &frame))) return;
            decodeBitmap(rt, frame.Get(), artwork_.ReleaseAndGetAddressOf());
            decodeBitmap(rt, frame.Get(), opaqueArtwork_.ReleaseAndGetAddressOf(), true);
            Microsoft::WRL::ComPtr<IWICBitmapScaler> scaler; Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
            if(SUCCEEDED(wic_->CreateBitmapScaler(&scaler))&&SUCCEEDED(scaler->Initialize(frame.Get(),1,1,WICBitmapInterpolationModeFant))&&SUCCEEDED(wic_->CreateFormatConverter(&converter))&&SUCCEEDED(converter->Initialize(scaler.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))){BYTE pixel[4]{};if(SUCCEEDED(converter->CopyPixels(nullptr,4,4,pixel))){float r=pixel[0],g=pixel[1],b=pixel[2],bright=std::max({r,g,b});if(bright<8.f){artworkAccent_=0x68D5C8;}else{float scale=std::clamp(235.f/bright,1.15f,3.25f);auto vivid=[scale](float channel){return static_cast<std::uint32_t>(std::clamp((channel-24.f)*scale+32.f,32.f,255.f));};artworkAccent_=(vivid(r)<<16)|(vivid(g)<<8)|vivid(b);}}}
            }

            std::array<float,64> OverlayPainter::delayedSpectrum(const std::array<float,64>& spectrum,std::uint32_t delayMs)
            {
              const auto now=GetTickCount64();spectrumHistory_.push_back({now,spectrum});
              while(spectrumHistory_.size()>2&&now-spectrumHistory_[1].tick>std::max<std::uint32_t>(delayMs,500u))spectrumHistory_.pop_front();
              if(!delayMs)return spectrum;
              const auto target=now>delayMs?now-delayMs:0;auto result=spectrumHistory_.front().bands;
              for(const auto& sample:spectrumHistory_){if(sample.tick>target)break;result=sample.bands;}
              return result;
              }

              void OverlayPainter::drawSpectrum(ID2D1RenderTarget* rt,ID2D1Bitmap* artwork,const Appearance& a,
              const std::array<float,64>& spectrum,float w,float h,std::uint32_t accentColor)
              {
                const std::size_t count=std::clamp<std::size_t>(a.spectrumBars,8,64);const float maxHeight=std::max(8.f,h*std::clamp(a.spectrumMaxHeight,10u,100u)/100.f);
                std::vector<D2D1_POINT_2F> points;points.reserve(count);
                for(std::size_t i=0;i<count;++i){const std::size_t source=count==1?0:(i*63)/(count-1);float level=std::clamp(spectrum[source],0.f,1.f);float barHeight=4.f+level*std::max(0.f,maxHeight-4.f);float x=count==1?w*.5f:float(i)*w/float(count-1);float y=a.corner<2?barHeight:h-barHeight;points.push_back(D2D1::Point2F(x,y));}

                Microsoft::WRL::ComPtr<ID2D1Factory> factory;rt->GetFactory(factory.ReleaseAndGetAddressOf());if(!factory)return;
                Microsoft::WRL::ComPtr<ID2D1Geometry> mask;
                if(a.spectrumCurves)
                {
                  Microsoft::WRL::ComPtr<ID2D1PathGeometry> path;Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;if(FAILED(factory->CreatePathGeometry(&path))||FAILED(path->Open(&sink)))return;
                  const float anchor=a.corner<2?0.f:h;if(a.corner==0||a.corner==2)points.back().y=anchor;else points.front().y=anchor;sink->BeginFigure(D2D1::Point2F(0,anchor),D2D1_FIGURE_BEGIN_FILLED);sink->AddLine(points.front());
                  for(std::size_t i=0;i+1<points.size();++i){const auto p0=points[i?i-1:i],p1=points[i],p2=points[i+1],p3=points[i+2<points.size()?i+2:i+1];sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(p1.x+(p2.x-p0.x)/6.f,p1.y+(p2.y-p0.y)/6.f),D2D1::Point2F(p2.x-(p3.x-p1.x)/6.f,p2.y-(p3.y-p1.y)/6.f),p2));}
                  sink->AddLine(D2D1::Point2F(w,anchor));sink->EndFigure(D2D1_FIGURE_END_CLOSED);sink->Close();mask=path;
                  }else
                  {
                    std::vector<Microsoft::WRL::ComPtr<ID2D1Geometry>> owned;std::vector<ID2D1Geometry*> raw;owned.reserve(count);raw.reserve(count);const float cell=w/float(count),gap=std::max(1.f,cell*.12f);
                    for(std::size_t i=0;i<count;++i){float level=std::clamp(spectrum[(i*63)/std::max<std::size_t>(1,count-1)],0.f,1.f);float barHeight=4.f+level*std::max(0.f,maxHeight-4.f);float top=a.corner<2?0.f:h-barHeight,bottom=a.corner<2?barHeight:h;Microsoft::WRL::ComPtr<ID2D1RoundedRectangleGeometry> bar;factory->CreateRoundedRectangleGeometry(D2D1::RoundedRect(D2D1::RectF(float(i)*cell+gap*.5f,top,float(i+1)*cell-gap*.5f,bottom),3,3),&bar);raw.push_back(bar.Get());owned.push_back(bar);}
                    Microsoft::WRL::ComPtr<ID2D1GeometryGroup> group;if(FAILED(factory->CreateGeometryGroup(D2D1_FILL_MODE_WINDING,raw.data(),static_cast<UINT32>(raw.size()),&group)))return;mask=group;
                    }

                    Microsoft::WRL::ComPtr<ID2D1Brush> fillBrush;Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> accentBrush;Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> chromaBrush;
                    if(a.spectrumChroma&&!a.artworkSpectrum)
                    {
                      D2D1_GRADIENT_STOP stops[7]{};const float shift=float((GetTickCount64()*a.spectrumHueSpeed/1000)%360);
                      for(UINT i=0;i<7;++i){float hue=std::fmod(shift+float(i)*60.f,360.f),c=1.f,x=c*(1.f-std::abs(std::fmod(hue/60.f,2.f)-1.f));float r{},g{},b{};if(hue<60){r=c;g=x;}else if(hue<120){r=x;g=c;}else if(hue<180){g=c;b=x;}else if(hue<240){g=x;b=c;}else if(hue<300){r=x;b=c;}else{r=c;b=x;}stops[i]={float(i)/6.f,D2D1::ColorF(r,g,b,.92f)};}
                      Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> collection;if(SUCCEEDED(rt->CreateGradientStopCollection(stops,7,&collection)))rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(0,0),D2D1::Point2F(w,0)),collection.Get(),&chromaBrush);fillBrush=chromaBrush;
                      }else if(!a.artworkSpectrum||!artwork){rt->CreateSolidColorBrush(D2D1::ColorF(accentColor,.9f),&accentBrush);fillBrush=accentBrush;}

                      Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> fadeBrush;
                      if(a.spectrumFade){const float fade=std::clamp(a.spectrumFade,1u,100u)/100.f;D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(0xffffff,0.f)},{fade,D2D1::ColorF(0xffffff,1.f)},{1,D2D1::ColorF(0xffffff,1.f)}};Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> collection;if(SUCCEEDED(rt->CreateGradientStopCollection(stops,3,&collection))){const bool growsDown=a.corner<2;rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(growsDown?D2D1::Point2F(0,h):D2D1::Point2F(0,0),growsDown?D2D1::Point2F(0,0):D2D1::Point2F(0,h)),collection.Get(),&fadeBrush);}}
                      Microsoft::WRL::ComPtr<ID2D1Layer> layer;rt->CreateLayer(nullptr,&layer);rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),mask.Get(),D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,D2D1::Matrix3x2F::Identity(),1.f,fadeBrush.Get()),layer.Get());
                      if(a.artworkSpectrum&&artwork){auto size=artwork->GetSize();float targetAspect=w/h,sourceAspect=size.width/std::max(1.f,size.height);D2D1_RECT_F source{};if(sourceAspect>targetAspect){float crop=size.height*targetAspect,left=(size.width-crop)*.5f;source=D2D1::RectF(left,0,left+crop,size.height);}else{float crop=size.width/targetAspect,top=(size.height-crop)*.5f;source=D2D1::RectF(0,top,size.width,top+crop);}rt->DrawBitmap(artwork,D2D1::RectF(0,0,w,h),.94f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,source);}else if(fillBrush)rt->FillRectangle(D2D1::RectF(0,0,w,h),fillBrush.Get());rt->PopLayer();
                      }

            void OverlayPainter::paint(ID2D1RenderTarget* rt, const FrameView& frame)
            {
              const auto& t = frame.track; const auto& a = frame.appearance;
              float w = static_cast<float>(frame.width), h = static_cast<float>(frame.height);
              SYSTEMTIME localTime{};
              GetLocalTime(&localTime);
              const bool aprilFools = localTime.wMonth == 4 && localTime.wDay == 1;
              updateArtwork(rt, t);
              ID2D1Bitmap* displayedArtwork = aprilFools || !opaqueArtwork_ ? artwork_.Get() : opaqueArtwork_.Get();
              updateLogo(rt, a);
              std::uint32_t accentColor=a.dynamicAccent?artworkAccent_:a.accentRgb;
              Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> panel, primary, secondary, accent, artworkBacking;
              rt->CreateSolidColorBrush(D2D1::ColorF(0x11151D, a.opacity), &panel);
              rt->CreateSolidColorBrush(D2D1::ColorF(0xF4F7FB, 1.f), &primary);
              rt->CreateSolidColorBrush(D2D1::ColorF(0xAAB4C3, 1.f), &secondary);
              rt->CreateSolidColorBrush(D2D1::ColorF(accentColor, 1.f), &accent);
              rt->CreateSolidColorBrush(D2D1::ColorF(0x11151D, 1.f), &artworkBacking);
              auto box = D2D1::RoundedRect(D2D1::RectF(0, 0, w, h), 18, 18);
              if(!a.overkill||a.settingsOpen)rt->FillRoundedRectangle(box, panel.Get());
              if((!a.overkill||a.settingsOpen)&&a.artworkBackground&&displayedArtwork)
              {
                float backgroundHeight=a.settingsOpen?108.f:h;
                Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory;Microsoft::WRL::ComPtr<ID2D1RoundedRectangleGeometry> clipGeometry;Microsoft::WRL::ComPtr<ID2D1Layer> clipLayer;
                rt->GetFactory(d2dFactory.ReleaseAndGetAddressOf());d2dFactory->CreateRoundedRectangleGeometry(D2D1::RoundedRect(D2D1::RectF(0,0,w,backgroundHeight),18,18),clipGeometry.ReleaseAndGetAddressOf());rt->CreateLayer(nullptr,clipLayer.ReleaseAndGetAddressOf());rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),clipGeometry.Get()),clipLayer.Get());
                if(aprilFools)
                {
                  // Intentionally cursed stretch, preserved as an April Fools' Day easter egg.
                  rt->DrawBitmap(displayedArtwork,D2D1::RectF(0,0,w,backgroundHeight),.22f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
                  }else
                  {
                    auto size=displayedArtwork->GetSize();float targetAspect=w/backgroundHeight,sourceAspect=size.width/std::max(1.f,size.height);D2D1_RECT_F source{};
                    if(sourceAspect>targetAspect){float croppedWidth=size.height*targetAspect;float left=(size.width-croppedWidth)*.5f;source=D2D1::RectF(left,0,left+croppedWidth,size.height);}else{float croppedHeight=size.width/targetAspect;float top=(size.height-croppedHeight)*.5f;source=D2D1::RectF(0,top,size.width,top+croppedHeight);}
                    rt->DrawBitmap(displayedArtwork,D2D1::RectF(0,0,w,backgroundHeight),.22f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,source);
                    }
                    rt->PopLayer();
                    }
                    float textLeft = 22.f;
                    if(a.overkill&&!a.settingsOpen)
                    {
                      drawSpectrum(rt,displayedArtwork,a,delayedSpectrum(frame.spectrum,a.spectrumDelayMs),w,h,accentColor);
                      const bool showArtworkTile=displayedArtwork&&!a.artworkSpectrum;if(showArtworkTile){const auto artRect=D2D1::RectF(16,14,122,120);if(!aprilFools)rt->FillRectangle(artRect,artworkBacking.Get());rt->DrawBitmap(displayedArtwork,artRect,1.f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);}float overkillLeft=showArtworkTile?140.f:22.f;std::wstring overkillTitle=t.title.empty()?L"Nothing playing":titleForDisplay(t.title,a.ignoreParenthetical);std::wstring overkillArtist=t.artist.empty()?L"Windows media session":t.artist;drawFittedText(rt,overkillTitle,D2D1::RectF(overkillLeft,18,w-22,78),28,13,primary.Get(),DWRITE_FONT_WEIGHT_SEMI_BOLD);drawFittedText(rt,overkillArtist,D2D1::RectF(overkillLeft,82,w-22,112),16,10,secondary.Get(),DWRITE_FONT_WEIGHT_NORMAL);if(a.showBanner&&logo_)rt->DrawBitmap(logo_.Get(),D2D1::RectF(18,128,168,174),.88f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);return;
                      }
                      if (displayedArtwork&&!a.artworkBackground)
                      {
                        float artBottom = a.compact ? 66.f : 94.f;
                        const auto artRect = D2D1::RectF(12, 10, 12+(artBottom-10), artBottom);
                        if(!aprilFools) rt->FillRectangle(artRect, artworkBacking.Get());
                        rt->DrawBitmap(displayedArtwork, artRect, 1.f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
                        textLeft = 18+(artBottom-10);
                        }
                        std::wstring title = t.title.empty() ? L"Nothing playing" : titleForDisplay(t.title,a.ignoreParenthetical);
                        std::wstring artist = t.artist.empty() ? L"Windows media session" : t.artist;
                        drawFittedText(rt,title,D2D1::RectF(textLeft,15,w-158,a.compact?55.f:51.f),22,11,primary.Get(),DWRITE_FONT_WEIGHT_SEMI_BOLD);
                        if (!a.compact) drawFittedText(rt,artist,D2D1::RectF(textLeft,53,w-125,79),14,9,secondary.Get(),DWRITE_FONT_WEIGHT_NORMAL);
                        if (a.showBanner&&logo_) rt->DrawBitmap(logo_.Get(), D2D1::RectF(w-148, 12, w-18, 56), .85f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
                        std::uint64_t now=GetTickCount64();
                        if(progressRevision_!=t.revision)
                        {
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
                          if (a.settingsOpen)
                          {
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
                            const wchar_t* parentheses=L"Ignore title parentheses";rt->DrawTextW(parentheses,24,detail_.Get(),D2D1::RectF(20,495,220,518),primary.Get());rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(w-70,495,w-24,517),11,11),accent.Get(),2);if(a.ignoreParenthetical)rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(w-36,506),7,7),accent.Get());
                            }
                            }
                            }
