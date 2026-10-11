// Settings window styled after the CyberiaStyle LAST APPROACH options screens:
// a dark teal shard field, an olive side panel and glass main panel, an angular
// header with hazard stripes and outlined page digits, amber row titles,
// horizontally listed choices with a teal bracket cursor, a gradient focus
// strip, and an explanation box. Everything is drawn with Direct2D primitives.
#include "SettingsWindow.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <commdlg.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwmapi.h>
#include <dwrite.h>
#include <dwrite_1.h>
#include <string>
#include <vector>
#include <windowsx.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
#ifdef small
#undef small
#endif

namespace mo::standalone {
namespace {
constexpr wchar_t kClass[] = L"NihonsMusicFrameworkSettings";
constexpr UINT_PTR kAnimationTimer = 1;
constexpr float kWidth = 880.f, kHeight = 760.f;
constexpr float kTabsTop = 132.f, kRowsTop = 190.f, kRowStep = 38.f;
constexpr float kTitleRight = 232.f, kValuesLeft = 290.f, kValuesRight = 852.f;

// Palette
constexpr UINT32 kBackground = 0x020B0B, kTealDeep = 0x062222, kTealDark = 0x0A3B3B, kTealMid = 0x127070, kCyan = 0x3FF6E6, kCyanLine = 0x18C8BC;
constexpr UINT32 kAmber = 0xFFB43C, kOrange = 0xE8691E, kOrangeDeep = 0xB8431A, kWhite = 0xE6F2F0, kValueGrey = 0xA9BDBD, kMuted = 0x6F8F8C, kOlive = 0x5A4A16;

enum Id : int {
  IdWindowed, IdOpacity, IdCompact, IdDynamicAccent, IdArtworkBackground, IdBanner, IdParentheses, IdRenderer, IdAccent, IdShortcut, IdFont,
  IdOverkill = 20, IdCorner, IdBars, IdResponse, IdDelay, IdHeight, IdFade, IdCurves, IdChroma, IdChromaRate, IdArtworkSpectrum,
  IdDiscord = 40, IdAppId, IdIconKey, IdArtworkKey,
};
enum class Kind { Choice, Meter, Accent, Shortcut, Font, Renderer, Text };
struct RowDef { Id id; Kind kind; const wchar_t* title; const wchar_t* help; };

const RowDef kGeneral[] = {
  {IdWindowed, Kind::Choice, L"Windowed Mode", L"Shows the overlay as a normal, movable window with a transparent background for OBS and other capture tools. Interaction stays enabled in this mode."},
  {IdOpacity, Kind::Meter, L"Transparency", L"Overall opacity of the overlay, including artwork, text, and spectra. Click or drag along the meter."},
  {IdCompact, Kind::Choice, L"Compact Mode", L"A smaller panel that shows the title and progress only."},
  {IdDynamicAccent, Kind::Choice, L"Dynamic Accent", L"Derives the accent color from the current album artwork instead of the fixed accent color."},
  {IdArtworkBackground, Kind::Choice, L"Artwork Background", L"Fills the panel with a dimmed, cropped copy of the album artwork."},
  {IdBanner, Kind::Choice, L"Show Banner", L"Displays the framework banner while interaction mode is off."},
  {IdParentheses, Kind::Choice, L"Ignore Parentheses", L"Hides parenthesized parts of track titles, such as (Remastered) or (feat. ...)."},
  {IdRenderer, Kind::Renderer, L"Renderer", L"Graphics backend used by the overlay. The change takes effect after restarting the application."},
  {IdAccent, Kind::Accent, L"Accent Color", L"Color used for the progress bar, media controls, and spectrum when Dynamic Accent is off."},
  {IdShortcut, Kind::Shortcut, L"Shortcut", L"Double-tap this key combination to toggle interaction mode. Click, then press the new combination."},
  {IdFont, Kind::Font, L"Overlay Font", L"Installed font family used for titles and artist names. Long text is still fitted automatically."},
};
const RowDef kOverkill[] = {
  {IdOverkill, Kind::Choice, L"Overkill Mode", L"Large corner display with a live spectrum of the playing application's audio."},
  {IdCorner, Kind::Choice, L"Screen Corner", L"Corner of the active monitor that Overkill mode docks to. Fullscreen applications are followed automatically."},
  {IdBars, Kind::Choice, L"Bars / Points", L"Number of spectrum bars, or curve points when Curves Style is on."},
  {IdResponse, Kind::Choice, L"Response", L"How quickly the spectrum reacts. Fast follows transients; Smooth eases them."},
  {IdDelay, Kind::Choice, L"Display Delay", L"Delays the spectrum to line up with audio latency from Bluetooth or streaming setups."},
  {IdHeight, Kind::Choice, L"Max Height", L"Maximum spectrum height as a share of the Overkill panel."},
  {IdFade, Kind::Choice, L"Top Fade", L"Fades the tips of the spectrum into transparency."},
  {IdCurves, Kind::Choice, L"Curves Style", L"Draws the spectrum as a smooth filled curve instead of bars."},
  {IdChroma, Kind::Choice, L"Chroma", L"Paints the spectrum with a rotating rainbow gradient."},
  {IdChromaRate, Kind::Choice, L"Chroma Rate", L"Hue rotation speed of the Chroma gradient."},
  {IdArtworkSpectrum, Kind::Choice, L"Artwork Spectrum", L"Uses the album artwork as the spectrum fill."},
};
const RowDef kDiscord[] = {
  {IdDiscord, Kind::Choice, L"Rich Presence", L"Publishes the current track to your Discord profile through the local Discord client."},
  {IdAppId, Kind::Text, L"Application ID", L"Discord application whose name and assets appear on your profile. Click to edit; Enter applies, Esc cancels."},
  {IdIconKey, Kind::Text, L"Default Icon Key", L"Asset key shown when nothing is playing or no artwork key is set."},
  {IdArtworkKey, Kind::Text, L"Artwork Key", L"Optional asset key shown while music plays. Discord cannot receive raw GSMTC artwork through classic RPC."},
};
struct Page { const RowDef* rows; int count; const wchar_t* name; };
const Page kPages[] = {{kGeneral, 11, L"GENERAL"}, {kOverkill, 11, L"OVERKILL"}, {kDiscord, 4, L"DISCORD RPC"}};

constexpr std::array<std::uint32_t, 5> kBars{16, 24, 32, 48, 64}, kDelays{0, 50, 100, 200, 350}, kFades{0, 5, 10, 20, 30};
constexpr std::array<std::uint32_t, 4> kHeights{25, 50, 75, 100}, kHues{15, 30, 60, 120};

struct Choices { const wchar_t* labels[6]{}; int count{}; int current{-1}; };
template <std::size_t N>
int indexOf(const std::array<std::uint32_t, N>& values, std::uint32_t value)
{
  for (std::size_t i = 0; i < N; ++i) if (values[i] == value) return static_cast<int>(i);
  return -1;
}
Choices toggle(bool value) { Choices c; c.labels[0] = L"OFF"; c.labels[1] = L"ON"; c.count = 2; c.current = value ? 1 : 0; return c; }

struct Hit { D2D1_RECT_F rect; int id; int choice; };
struct Anim { int id; float x, w; };
D2D1_COLOR_F rgba(UINT32 rgb, float a = 1.f) { return D2D1::ColorF(rgb, a); }
float lerp(float a, float b, float t) { return a + (b - a) * t; }
}

struct SettingsWindow::Impl
{
  ComPtr<ID2D1Factory> factory;
  ComPtr<ID2D1HwndRenderTarget> target;
  ComPtr<IDWriteFactory> write;
  ComPtr<IDWriteTextFormat> title, caption, tab, rowTitle, value, small, digits, help, button, field;
  ComPtr<ID2D1SolidColorBrush> solid;
  ComPtr<ID2D1LinearGradientBrush> amber, panel, strip, chrome, accentGlow;
  std::wstring family{L"Bahnschrift"};
  int page{}, edit{-1}, hover{-1};
  float highlightY{-1.f}, dpi{96.f};
  bool dragging{}, tracking{};
  ULONGLONG started{};
  std::vector<Hit> hits;
  std::vector<Anim> anims;

  // ---- drawing helpers --------------------------------------------------
  ID2D1SolidColorBrush* color(UINT32 rgb, float a = 1.f) { solid->SetColor(rgba(rgb, a)); return solid.Get(); }

  ComPtr<IDWriteTextLayout> layout(const std::wstring& s, IDWriteTextFormat* format, float width, float height, float spacing,
                                   DWRITE_TEXT_ALIGNMENT align, DWRITE_PARAGRAPH_ALIGNMENT paragraph)
  {
    ComPtr<IDWriteTextLayout> result;
    if (FAILED(write->CreateTextLayout(s.c_str(), static_cast<UINT32>(s.size()), format, std::max(1.f, width), std::max(1.f, height), &result))) return {};
    result->SetTextAlignment(align);
    result->SetParagraphAlignment(paragraph);
    if (format != help.Get()) result->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    if (spacing != 0.f)
    {
      ComPtr<IDWriteTextLayout1> spaced;
      if (SUCCEEDED(result.As(&spaced))) spaced->SetCharacterSpacing(0.f, spacing, 0.f, DWRITE_TEXT_RANGE{0, static_cast<UINT32>(s.size())});
    }
    return result;
  }
  void text(const std::wstring& s, IDWriteTextFormat* format, D2D1_RECT_F box, ID2D1Brush* brush, float spacing = 0.f,
            DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT paragraph = DWRITE_PARAGRAPH_ALIGNMENT_CENTER)
  {
    auto l = layout(s, format, box.right - box.left, box.bottom - box.top, spacing, align, paragraph);
    if (l) target->DrawTextLayout(D2D1::Point2F(box.left, box.top), l.Get(), brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
  }
  // Text with a one-pixel dark drop stroke, like the theme's stroked bitmap fonts.
  void strokedText(const std::wstring& s, IDWriteTextFormat* format, D2D1_RECT_F box, ID2D1Brush* brush, float spacing = 0.f,
                   DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING)
  {
    auto l = layout(s, format, box.right - box.left, box.bottom - box.top, spacing, align, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    if (!l) return;
    auto* shadow = color(0x000000, .85f);
    target->DrawTextLayout(D2D1::Point2F(box.left + 1.2f, box.top + 1.2f), l.Get(), shadow);
    target->DrawTextLayout(D2D1::Point2F(box.left - .6f, box.top + .6f), l.Get(), shadow);
    target->DrawTextLayout(D2D1::Point2F(box.left, box.top), l.Get(), brush);
  }
  // Outlined digits: draw the text around the origin in the line color, then knock out the middle.
  void outlinedText(const std::wstring& s, IDWriteTextFormat* format, D2D1_RECT_F box, UINT32 line, float lineAlpha, UINT32 fill, float fillAlpha)
  {
    auto l = layout(s, format, box.right - box.left, box.bottom - box.top, 0.f, DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    if (!l) return;
    const float offsets[8][2] = {{-1.4f, 0}, {1.4f, 0}, {0, -1.4f}, {0, 1.4f}, {-1, -1}, {1, 1}, {-1, 1}, {1, -1}};
    for (auto& o : offsets) target->DrawTextLayout(D2D1::Point2F(box.left + o[0], box.top + o[1]), l.Get(), color(line, lineAlpha));
    target->DrawTextLayout(D2D1::Point2F(box.left, box.top), l.Get(), color(fill, fillAlpha));
  }
  float measure(const std::wstring& s, IDWriteTextFormat* format, float spacing = 0.f)
  {
    auto l = layout(s, format, 2000.f, 100.f, spacing, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    DWRITE_TEXT_METRICS metrics{};
    if (l) l->GetMetrics(&metrics);
    return metrics.widthIncludingTrailingWhitespace;
  }
  ComPtr<ID2D1PathGeometry> polygon(const D2D1_POINT_2F* points, int count)
  {
    ComPtr<ID2D1PathGeometry> path;
    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(factory->CreatePathGeometry(&path)) || FAILED(path->Open(&sink))) return {};
    sink->BeginFigure(points[0], D2D1_FIGURE_BEGIN_FILLED);
    for (int i = 1; i < count; ++i) sink->AddLine(points[i]);
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    return path;
  }
  void fillPolygon(std::initializer_list<D2D1_POINT_2F> points, ID2D1Brush* brush)
  {
    std::vector<D2D1_POINT_2F> list(points);
    if (auto path = polygon(list.data(), static_cast<int>(list.size()))) target->FillGeometry(path.Get(), brush);
  }
  void strokePolygon(std::initializer_list<D2D1_POINT_2F> points, ID2D1Brush* brush, float width)
  {
    std::vector<D2D1_POINT_2F> list(points);
    if (auto path = polygon(list.data(), static_cast<int>(list.size()))) target->DrawGeometry(path.Get(), brush, width);
  }
  // Parallelogram leaning right by `slant` pixels, the theme's basic shard shape.
  void shard(float x, float y, float w, float h, float slant, ID2D1Brush* brush)
  {
    fillPolygon({D2D1::Point2F(x + slant, y), D2D1::Point2F(x + w + slant, y), D2D1::Point2F(x + w, y + h), D2D1::Point2F(x, y + h)}, brush);
  }
  void glowLine(float x1, float y, float x2, UINT32 rgb, float alpha)
  {
    target->DrawLine(D2D1::Point2F(x1, y), D2D1::Point2F(x2, y), color(rgb, alpha * .18f), 5.f);
    target->DrawLine(D2D1::Point2F(x1, y), D2D1::Point2F(x2, y), color(rgb, alpha * .35f), 2.5f);
    target->DrawLine(D2D1::Point2F(x1, y), D2D1::Point2F(x2, y), color(rgb, alpha), 1.f);
  }
  void glowPill(float x, float y, float w, float h, UINT32 rgb, float pulse)
  {
    for (int i = 3; i >= 1; --i)
    {
      const float grow = static_cast<float>(i) * 2.f;
      target->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x - grow, y - grow, x + w + grow, y + h + grow), h, h), color(rgb, .07f * pulse));
    }
    target->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + h), h * .5f, h * .5f), color(rgb, .95f));
    target->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x + 2, y + 1, x + w - 2, y + h * .45f), h * .3f, h * .3f), color(0xFFF2C8, .55f));
  }
  ComPtr<ID2D1LinearGradientBrush> gradient(std::initializer_list<D2D1_GRADIENT_STOP> stops)
  {
    std::vector<D2D1_GRADIENT_STOP> list(stops);
    ComPtr<ID2D1GradientStopCollection> collection;
    ComPtr<ID2D1LinearGradientBrush> brush;
    if (SUCCEEDED(target->CreateGradientStopCollection(list.data(), static_cast<UINT32>(list.size()), &collection)))
      target->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(0, 1)), collection.Get(), &brush);
    return brush;
  }
  void createDeviceResources()
  {
    target->CreateSolidColorBrush(rgba(kWhite), &solid);
    amber = gradient({{0.f, rgba(0xFFD27A)}, {.55f, rgba(kAmber)}, {1.f, rgba(kOrangeDeep)}});
    panel = gradient({{0.f, rgba(0x2E3434, .78f)}, {.55f, rgba(0x1D4C4A, .74f)}, {1.f, rgba(0x1B6763, .70f)}});
    strip = gradient({{0.f, rgba(0x6AAA1E, .0f)}, {.12f, rgba(0x6AAA1E, .55f)}, {.45f, rgba(0x0A6A5A, .55f)}, {.75f, rgba(0x075A7A, .55f)}, {1.f, rgba(0x5FA040, .1f)}});
    chrome = gradient({{0.f, rgba(0xFFFFFF)}, {.5f, rgba(0xD6E2E2)}, {.52f, rgba(0x93A8A8)}, {1.f, rgba(0xDDE8E8)}});
    accentGlow = gradient({{0.f, rgba(kCyan, .0f)}, {.5f, rgba(kCyan, .9f)}, {1.f, rgba(kCyan, .0f)}});
  }
  void releaseDeviceResources() { solid.Reset(); amber.Reset(); panel.Reset(); strip.Reset(); chrome.Reset(); accentGlow.Reset(); target.Reset(); }

  Anim& anim(int id, float x, float w)
  {
    for (auto& a : anims) if (a.id == id) return a;
    anims.push_back({id, x, w});
    return anims.back();
  }
};

SettingsWindow::SettingsWindow() : impl_(std::make_unique<Impl>()) {}
SettingsWindow::~SettingsWindow() = default;

bool SettingsWindow::initialize(HINSTANCE instance, HICON icon, SettingsBindings bindings)
{
  instance_ = instance; icon_ = icon; bindings_ = bindings;
  auto& d = *impl_;
  if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d.factory.ReleaseAndGetAddressOf())) ||
      FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(d.write.ReleaseAndGetAddressOf()))))
    return false;

  // The theme uses a geometric sans; prefer Century Gothic and fall back to Bahnschrift.
  ComPtr<IDWriteFontCollection> fonts;
  if (SUCCEEDED(d.write->GetSystemFontCollection(&fonts)))
  {
    UINT32 index{}; BOOL exists{};
    if (SUCCEEDED(fonts->FindFamilyName(L"Century Gothic", &index, &exists)) && exists) d.family = L"Century Gothic";
  }
  const wchar_t* geometric = d.family.c_str();
  auto format = [&](const wchar_t* family, DWRITE_FONT_WEIGHT weight, DWRITE_FONT_STYLE style, float size, ComPtr<IDWriteTextFormat>& out)
  {
    d.write->CreateTextFormat(family, nullptr, weight, style, DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &out);
  };
  format(geometric, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, 30.f, d.title);
  format(geometric, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, 9.5f, d.caption);
  format(geometric, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, 15.f, d.tab);
  format(geometric, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, 16.5f, d.rowTitle);
  format(geometric, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, 13.5f, d.value);
  format(geometric, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, 11.f, d.small);
  format(geometric, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_OBLIQUE, 58.f, d.digits);
  format(L"Bahnschrift", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, 13.5f, d.help);
  format(geometric, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, 16.f, d.button);
  format(L"Consolas", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, 14.f, d.field);
  if (d.help) { d.help->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP); d.help->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, 20.f, 15.f); }

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc); wc.lpfnWndProc = windowProc; wc.hInstance = instance; wc.hIcon = icon; wc.hIconSm = icon;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.lpszClassName = kClass;
  if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
  hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW, kClass, L"Nihon's Music Framework \u2014 System Options", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                          CW_USEDEFAULT, CW_USEDEFAULT, 900, 800, nullptr, nullptr, instance, this);
  if (!hwnd_) return false;
  // Dark caption to match the panel (attribute 20 = immersive dark mode, 35 = caption color on Windows 11).
  const BOOL dark = TRUE;
  DwmSetWindowAttribute(hwnd_, 20, &dark, sizeof(dark));
  const COLORREF caption = RGB(0x03, 0x14, 0x14);
  DwmSetWindowAttribute(hwnd_, 35, &caption, sizeof(caption));
  fitToDpi();
  return true;
}

void SettingsWindow::fitToDpi()
{
  if (!hwnd_) return;
  const UINT dpi = GetDpiForWindow(hwnd_);
  impl_->dpi = static_cast<float>(dpi ? dpi : 96);
  RECT rect{0, 0, static_cast<LONG>(kWidth * impl_->dpi / 96.f), static_cast<LONG>(kHeight * impl_->dpi / 96.f)};
  AdjustWindowRectExForDpi(&rect, static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_STYLE)), FALSE, static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_EXSTYLE)), dpi);
  SetWindowPos(hwnd_, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  if (impl_->target) impl_->target->SetDpi(impl_->dpi, impl_->dpi);
}

void SettingsWindow::show(HWND owner)
{
  if (!hwnd_) return;
  SetWindowLongPtrW(hwnd_, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(owner));
  impl_->started = GetTickCount64();
  SetTimer(hwnd_, kAnimationTimer, 16, nullptr);
  refresh();
  ShowWindow(hwnd_, SW_SHOW);
  SetForegroundWindow(hwnd_);
}
void SettingsWindow::refresh() { if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE); }
void SettingsWindow::refreshShortcut() { refresh(); }
void SettingsWindow::apply() { if (bindings_.apply) bindings_.apply(bindings_.context); refresh(); }

void SettingsWindow::chooseAccent()
{
  CHOOSECOLORW c{sizeof(c)};
  static COLORREF custom[16]{};
  auto rgb = bindings_.appearance->accentRgb;
  c.hwndOwner = hwnd_; c.rgbResult = RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255); c.lpCustColors = custom; c.Flags = CC_FULLOPEN | CC_RGBINIT;
  if (ChooseColorW(&c)) { bindings_.appearance->accentRgb = (GetRValue(c.rgbResult) << 16) | (GetGValue(c.rgbResult) << 8) | GetBValue(c.rgbResult); apply(); }
}

void SettingsWindow::chooseFont()
{
  if (!bindings_.fontFamily) return;
  LOGFONTW font{};
  lstrcpynW(font.lfFaceName, bindings_.fontFamily->c_str(), LF_FACESIZE);
  font.lfCharSet = DEFAULT_CHARSET;
  CHOOSEFONTW choice{sizeof(choice)};
  choice.hwndOwner = hwnd_; choice.lpLogFont = &font; choice.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_FORCEFONTEXIST;
  if (ChooseFontW(&choice)) { *bindings_.fontFamily = font.lfFaceName; bindings_.appearance->fontFamily = bindings_.fontFamily->c_str(); apply(); }
}

namespace {
Choices choicesFor(int id, const SettingsBindings& b)
{
  const auto& a = *b.appearance;
  Choices c;
  switch (id)
  {
    case IdWindowed: return toggle(*b.windowedMode);
    case IdCompact: return toggle(a.compact);
    case IdDynamicAccent: return toggle(a.dynamicAccent);
    case IdArtworkBackground: return toggle(a.artworkBackground);
    case IdBanner: return toggle(a.showBanner);
    case IdParentheses: return toggle(a.ignoreParenthetical);
    case IdOverkill: return toggle(a.overkill);
    case IdCurves: return toggle(a.spectrumCurves);
    case IdChroma: return toggle(a.spectrumChroma);
    case IdArtworkSpectrum: return toggle(a.artworkSpectrum);
    case IdDiscord: return toggle(b.discord->enabled);
    case IdRenderer:
      c = {{L"DIRECTX 9", L"DIRECTX 10", L"DIRECTX 11", L"DIRECTX 12"}, 4, *b.backend == 9 ? 0 : *b.backend == 10 ? 1 : *b.backend == 12 ? 3 : 2};
      return c;
    case IdCorner: c = {{L"TOP LEFT", L"TOP RIGHT", L"BOTTOM LEFT", L"BOTTOM RIGHT"}, 4, static_cast<int>(std::min<std::uint32_t>(a.corner, 3))}; return c;
    case IdBars: c = {{L"16", L"24", L"32", L"48", L"64"}, 5, indexOf(kBars, a.spectrumBars)}; return c;
    case IdResponse: c = {{L"FAST", L"BALANCED", L"SMOOTH"}, 3, static_cast<int>(std::min<std::uint32_t>(a.spectrumResponse, 2))}; return c;
    case IdDelay: c = {{L"0 MS", L"50 MS", L"100 MS", L"200 MS", L"350 MS"}, 5, indexOf(kDelays, a.spectrumDelayMs)}; return c;
    case IdHeight: c = {{L"25%", L"50%", L"75%", L"100%"}, 4, indexOf(kHeights, a.spectrumMaxHeight)}; return c;
    case IdFade: c = {{L"0%", L"5%", L"10%", L"20%", L"30%"}, 5, indexOf(kFades, a.spectrumFade)}; return c;
    case IdChromaRate: c = {{L"15\u00B0/S", L"30\u00B0/S", L"60\u00B0/S", L"120\u00B0/S"}, 4, indexOf(kHues, a.spectrumHueSpeed)}; return c;
    default: return c;
  }
}

void selectChoice(int id, int index, SettingsBindings& b)
{
  auto& a = *b.appearance;
  const bool on = index == 1;
  switch (id)
  {
    case IdWindowed: *b.windowedMode = on; break;
    case IdCompact: a.compact = on; break;
    case IdDynamicAccent: a.dynamicAccent = on; break;
    case IdArtworkBackground: a.artworkBackground = on; break;
    case IdBanner: a.showBanner = on; break;
    case IdParentheses: a.ignoreParenthetical = on; break;
    case IdOverkill: a.overkill = on; break;
    case IdCurves: a.spectrumCurves = on; break;
    case IdChroma: a.spectrumChroma = on; break;
    case IdArtworkSpectrum: a.artworkSpectrum = on; break;
    case IdDiscord: b.discord->enabled = on; break;
    case IdRenderer: *b.backend = 9 + index; break;
    case IdCorner: a.corner = static_cast<std::uint32_t>(index); break;
    case IdBars: a.spectrumBars = kBars[static_cast<std::size_t>(index)]; break;
    case IdResponse: a.spectrumResponse = static_cast<std::uint32_t>(index); break;
    case IdDelay: a.spectrumDelayMs = kDelays[static_cast<std::size_t>(index)]; break;
    case IdHeight: a.spectrumMaxHeight = kHeights[static_cast<std::size_t>(index)]; break;
    case IdFade: a.spectrumFade = kFades[static_cast<std::size_t>(index)]; break;
    case IdChromaRate: a.spectrumHueSpeed = kHues[static_cast<std::size_t>(index)]; break;
    default: break;
  }
}

std::wstring* textField(int id, SettingsBindings& b)
{
  if (id == IdAppId) return &b.discord->applicationId;
  if (id == IdIconKey) return &b.discord->defaultIconKey;
  if (id == IdArtworkKey) return &b.discord->artworkKey;
  return nullptr;
}
}

void SettingsWindow::paint()
{
  PAINTSTRUCT ps{};
  BeginPaint(hwnd_, &ps);
  auto& d = *impl_;
  RECT client{};
  GetClientRect(hwnd_, &client);
  const auto pixelSize = D2D1::SizeU(static_cast<UINT32>(client.right), static_cast<UINT32>(client.bottom));
  if (!d.target)
  {
    d.factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(), d.dpi, d.dpi),
                                      D2D1::HwndRenderTargetProperties(hwnd_, pixelSize), &d.target);
    if (d.target) d.createDeviceResources();
  }
  else d.target->Resize(pixelSize);
  if (!d.target || !d.solid) { EndPaint(hwnd_, &ps); return; }
  auto* rt = d.target.Get();
  const float W = kWidth, H = kHeight;
  const float time = static_cast<float>(GetTickCount64() - d.started) / 1000.f;
  const float pulse = .75f + .25f * std::sin(time * 3.2f);
  d.hits.clear();

  rt->BeginDraw();
  rt->SetTransform(D2D1::Matrix3x2F::Identity());
  rt->Clear(rgba(kBackground));

  // ---- background shard field -------------------------------------------
  struct ShardDef { float x, y, w, h, slant; UINT32 rgb; float a; };
  static const ShardDef shards[] = {
    {-60, 40, 380, 46, 40, kTealDark, .55f}, {520, 18, 420, 30, -30, kTealDark, .6f}, {600, 96, 300, 14, -20, kTealMid, .35f},
    {-40, 610, 520, 22, 30, kTealDark, .6f}, {360, 650, 560, 34, -40, kTealDark, .5f}, {700, 560, 220, 90, -60, kTealMid, .25f},
    {40, 700, 300, 10, 20, kTealMid, .4f}, {780, 160, 140, 420, -90, kTealDark, .45f}, {820, 300, 80, 220, -50, kTealMid, .25f},
    {-30, 120, 120, 14, 20, kTealMid, .3f}, {150, 4, 260, 8, 18, kTealMid, .35f},
  };
  for (const auto& s : shards) d.shard(s.x, s.y, s.w, s.h, s.slant, d.color(s.rgb, s.a));

  // Olive side panel with diagonal slabs, as on the options page frame.
  d.fillPolygon({D2D1::Point2F(16, 176), D2D1::Point2F(262, 176), D2D1::Point2F(262, 612), D2D1::Point2F(16, 612)}, d.color(kOlive, .28f));
  for (int i = 0; i < 7; ++i)
  {
    const float x = -120.f + static_cast<float>(i) * 62.f;
    d.fillPolygon({D2D1::Point2F(x + 150, 176), D2D1::Point2F(x + 190, 176), D2D1::Point2F(x + 40, 612), D2D1::Point2F(x, 612)}, d.color(i % 2 ? 0x6A8A2A : 0x3E8A6A, .08f));
  }
  // Circuit traces on the side panel.
  d.strokePolygon({D2D1::Point2F(24, 560), D2D1::Point2F(120, 560), D2D1::Point2F(140, 540), D2D1::Point2F(250, 540)}, d.color(kCyanLine, .35f), 1.f);
  d.strokePolygon({D2D1::Point2F(30, 584), D2D1::Point2F(170, 584), D2D1::Point2F(186, 570), D2D1::Point2F(250, 570)}, d.color(kCyanLine, .22f), 1.f);

  // Glass main panel.
  d.panel->SetStartPoint(D2D1::Point2F(0, 176));
  d.panel->SetEndPoint(D2D1::Point2F(0, 612));
  d.fillPolygon({D2D1::Point2F(262, 176), D2D1::Point2F(850, 176), D2D1::Point2F(866, 192), D2D1::Point2F(866, 600), D2D1::Point2F(852, 612), D2D1::Point2F(262, 612)}, d.panel.Get());
  d.strokePolygon({D2D1::Point2F(262, 176), D2D1::Point2F(850, 176), D2D1::Point2F(866, 192), D2D1::Point2F(866, 600), D2D1::Point2F(852, 612), D2D1::Point2F(262, 612)}, d.color(kCyanLine, .35f), 1.f);
  // Ragged shards over the panel edges.
  for (int i = 0; i < 9; ++i)
  {
    const float x = 300.f + static_cast<float>(i) * 64.f;
    d.shard(x, 170.f + static_cast<float>(i % 3) * 3.f, 40.f + static_cast<float>((i * 13) % 30), 6.f, 8.f, d.color(i % 2 ? kTealMid : 0x0C2A2A, .8f));
    d.shard(x - 20.f, 606.f - static_cast<float>(i % 2) * 3.f, 50.f + static_cast<float>((i * 7) % 24), 7.f, -8.f, d.color(i % 3 ? kTealMid : 0x0C2A2A, .7f));
  }
  // Scanlines.
  for (float y = 178.f; y < 610.f; y += 3.f) rt->DrawLine(D2D1::Point2F(264, y), D2D1::Point2F(864, y), d.color(0x000000, .10f), 1.f);

  // ---- header ------------------------------------------------------------
  d.fillPolygon({D2D1::Point2F(0, 8), D2D1::Point2F(556, 8), D2D1::Point2F(586, 38), D2D1::Point2F(560, 70), D2D1::Point2F(0, 70)}, d.color(0x041616, .92f));
  d.strokePolygon({D2D1::Point2F(0, 8), D2D1::Point2F(556, 8), D2D1::Point2F(586, 38), D2D1::Point2F(560, 70), D2D1::Point2F(0, 70)}, d.color(kCyanLine, .5f), 1.f);
  d.glowLine(380, 12, 560, kCyan, .9f);
  d.text(L"NIHON'S MUSIC FRAMEWORK  //  LAST APPROACH STYLE", d.caption.Get(), D2D1::RectF(30, 12, 520, 26), d.color(kMuted), 1.5f);
  d.chrome->SetStartPoint(D2D1::Point2F(0, 26));
  d.chrome->SetEndPoint(D2D1::Point2F(0, 64));
  d.strokedText(L"SYSTEM OPTIONS", d.title.Get(), D2D1::RectF(28, 24, 560, 66), d.chrome.Get(), 3.f);
  // Hazard stripes and amber glow pills.
  for (int i = 0; i < 20; ++i)
  {
    const float x = 170.f + static_cast<float>(i) * 9.f;
    d.shard(x, 74, 5, 8, 5, d.color(kOrange, .75f));
  }
  d.glowPill(32, 74, 90, 8, kAmber, pulse);
  d.glowPill(380, 75, 48, 5, kAmber, pulse);
  d.shard(440, 72, 120, 10, 10, d.color(kTealMid, .6f));

  // Right header plate with outlined page digits.
  d.fillPolygon({D2D1::Point2F(600, 8), D2D1::Point2F(880, 8), D2D1::Point2F(880, 86), D2D1::Point2F(640, 86), D2D1::Point2F(620, 64)}, d.color(0x041616, .85f));
  d.strokePolygon({D2D1::Point2F(600, 8), D2D1::Point2F(880, 8), D2D1::Point2F(880, 86), D2D1::Point2F(640, 86), D2D1::Point2F(620, 64)}, d.color(kCyanLine, .4f), 1.f);
  d.text(L"PAGE", d.caption.Get(), D2D1::RectF(760, 10, 860, 24), d.color(kAmber), 2.f, DWRITE_TEXT_ALIGNMENT_TRAILING);
  wchar_t digits[8]{};
  wsprintfW(digits, L"%02d", d.page + 1);
  d.outlinedText(digits, d.digits.Get(), D2D1::RectF(660, 14, 862, 88), 0x9EB4B4, .9f, 0x041818, 1.f);
  d.text(L"/ 03", d.small.Get(), D2D1::RectF(640, 66, 700, 84), d.color(kMuted), 1.f);
  d.text(kPages[d.page].name, d.caption.Get(), D2D1::RectF(636, 26, 780, 42), d.color(kValueGrey), 2.f);
  for (int i = 0; i < 3; ++i) d.shard(636.f + static_cast<float>(i) * 22.f, 46.f, 16.f, 5.f, 4.f, d.color(i == d.page ? kAmber : kTealMid, i == d.page ? .95f : .6f));
  d.glowLine(20, 92, 860, kCyanLine, .55f);
  d.glowLine(640, 92, 760, kCyan, pulse);

  // ---- information line -----------------------------------------------------
  d.text(L"INFORMATION", d.caption.Get(), D2D1::RectF(28, 100, 140, 122), d.color(kOrange), 2.f);
  rt->DrawLine(D2D1::Point2F(128, 120), D2D1::Point2F(140, 102), d.color(kOrange), 1.5f);
  d.text(L"Click a value to change it. Changes apply immediately and are saved automatically.", d.help.Get(), D2D1::RectF(152, 100, 860, 122), d.color(kValueGrey));

  // ---- tabs -----------------------------------------------------------------
  for (int i = 0; i < 3; ++i)
  {
    const float x = 28.f + static_cast<float>(i) * 272.f, w = 256.f;
    const bool active = d.page == i;
    const D2D1_POINT_2F plate[] = {D2D1::Point2F(x + 14, kTabsTop), D2D1::Point2F(x + w, kTabsTop), D2D1::Point2F(x + w - 14, kTabsTop + 34), D2D1::Point2F(x, kTabsTop + 34)};
    if (auto path = d.polygon(plate, 4))
    {
      if (active)
      {
        d.strip->SetStartPoint(D2D1::Point2F(x, 0));
        d.strip->SetEndPoint(D2D1::Point2F(x + w, 0));
        rt->FillGeometry(path.Get(), d.color(0x041616, .95f));
        rt->FillGeometry(path.Get(), d.strip.Get());
      }
      else rt->FillGeometry(path.Get(), d.color(0x041616, .8f));
      rt->DrawGeometry(path.Get(), d.color(active ? kCyan : kTealMid, active ? .9f : .6f), active ? 1.5f : 1.f);
    }
    wchar_t index[4]{};
    wsprintfW(index, L"%02d", i + 1);
    d.text(index, d.caption.Get(), D2D1::RectF(x + 22, kTabsTop + 2, x + 60, kTabsTop + 16), d.color(active ? kAmber : kMuted), 1.f);
    d.strokedText(kPages[i].name, d.tab.Get(), D2D1::RectF(x + 22, kTabsTop + 8, x + w - 20, kTabsTop + 32), d.color(active ? kWhite : kValueGrey, active ? 1.f : .8f), 2.f);
    if (active) d.glowPill(x + w - 60, kTabsTop + 28, 30, 4, kAmber, pulse);
    d.hits.push_back({D2D1::RectF(x, kTabsTop, x + w, kTabsTop + 34), -100 - i, -1});
  }

  // ---- option rows ----------------------------------------------------------
  const Page& page = kPages[d.page];
  int hoveredRow = -1;
  for (int r = 0; r < page.count; ++r) if (page.rows[r].id == d.hover) hoveredRow = r;
  if (hoveredRow >= 0)
  {
    const float targetY = kRowsTop + static_cast<float>(hoveredRow) * kRowStep;
    d.highlightY = d.highlightY < 0 ? targetY : lerp(d.highlightY, targetY, .35f);
    const float y = d.highlightY;
    d.strip->SetStartPoint(D2D1::Point2F(24, 0));
    d.strip->SetEndPoint(D2D1::Point2F(866, 0));
    d.fillPolygon({D2D1::Point2F(40, y + 2), D2D1::Point2F(866, y + 2), D2D1::Point2F(852, y + 34), D2D1::Point2F(24, y + 34)}, d.color(0x000000, .45f));
    d.fillPolygon({D2D1::Point2F(40, y + 2), D2D1::Point2F(866, y + 2), D2D1::Point2F(852, y + 34), D2D1::Point2F(24, y + 34)}, d.strip.Get());
    d.glowLine(40, y + 2, 866, kCyan, .7f);
    rt->DrawLine(D2D1::Point2F(24, y + 34), D2D1::Point2F(852, y + 34), d.color(kCyanLine, .5f), 1.f);
  }
  else d.highlightY = -1.f;

  auto& b = bindings_;
  auto& a = *b.appearance;
  for (int r = 0; r < page.count; ++r)
  {
    const RowDef& row = page.rows[r];
    const float y = kRowsTop + static_cast<float>(r) * kRowStep, mid = y + 18.f;
    const bool hovered = row.id == d.hover;
    // Row title: amber gradient, right-aligned against the bullet column.
    d.amber->SetStartPoint(D2D1::Point2F(0, y + 6));
    d.amber->SetEndPoint(D2D1::Point2F(0, y + 30));
    d.strokedText(row.title, d.rowTitle.Get(), D2D1::RectF(20, y, kTitleRight, y + 36), hovered ? static_cast<ID2D1Brush*>(d.color(0xFFE6A8)) : d.amber.Get(), .5f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    // Bullet chevron.
    const float bx = 248.f;
    d.fillPolygon({D2D1::Point2F(bx - 6, mid - 4), D2D1::Point2F(bx + 6, mid - 4), D2D1::Point2F(bx, mid + 5)}, d.color(hovered ? kOrange : 0x8FA3A3, hovered ? pulse : .8f));
    if (hovered) d.fillPolygon({D2D1::Point2F(bx - 10, mid - 7), D2D1::Point2F(bx + 10, mid - 7), D2D1::Point2F(bx, mid + 8)}, d.color(kOrange, .18f * pulse));
    d.hits.push_back({D2D1::RectF(20, y, kValuesRight, y + 36), row.id, -1});

    switch (row.kind)
    {
      case Kind::Choice:
      case Kind::Renderer:
      {
        if (row.kind == Kind::Renderer && b.rendererName)
        {
          // SDL frontend: a single value cycled with arrows.
          const std::wstring name = b.rendererName(b.context);
          const float w = d.measure(name, d.value.Get(), 1.f) + 28.f;
          d.text(L"\u25C0", d.small.Get(), D2D1::RectF(kValuesLeft, y, kValuesLeft + 20, y + 36), d.color(kOrange, pulse));
          const float x = kValuesLeft + 26.f;
          auto& an = d.anim(row.id, x, w);
          an.x = lerp(an.x, x, .3f); an.w = lerp(an.w, w, .3f);
          d.strokePolygon({D2D1::Point2F(an.x + 8, mid - 12), D2D1::Point2F(an.x + an.w, mid - 12), D2D1::Point2F(an.x + an.w, mid + 6), D2D1::Point2F(an.x + an.w - 8, mid + 12), D2D1::Point2F(an.x, mid + 12), D2D1::Point2F(an.x, mid - 4)}, d.color(kCyan, .9f), 1.5f);
          d.text(name, d.value.Get(), D2D1::RectF(x + 14, y, x + w, y + 36), d.color(kWhite), 1.f);
          d.text(L"\u25B6", d.small.Get(), D2D1::RectF(x + w + 8, y, x + w + 28, y + 36), d.color(kOrange, pulse));
          d.hits.push_back({D2D1::RectF(kValuesLeft, y, x + w + 30, y + 36), row.id, -2});
          break;
        }
        const Choices c = choicesFor(row.id, b);
        float x = kValuesLeft;
        for (int i = 0; i < c.count; ++i)
        {
          const std::wstring label = c.labels[i];
          const float w = d.measure(label, d.value.Get(), 1.5f);
          const bool selected = i == c.current;
          if (selected)
          {
            // Teal bracket cursor that glides between choices.
            auto& an = d.anim(row.id, x - 12.f, w + 24.f);
            an.x = lerp(an.x, x - 12.f, .3f); an.w = lerp(an.w, w + 24.f, .3f);
            const float l = an.x, rgt = an.x + an.w;
            d.fillPolygon({D2D1::Point2F(l, mid - 12), D2D1::Point2F(rgt, mid - 12), D2D1::Point2F(rgt, mid + 12), D2D1::Point2F(l, mid + 12)}, d.color(0x000000, .35f));
            auto* line = d.color(kCyan, .95f);
            rt->DrawLine(D2D1::Point2F(l + 8, mid - 12), D2D1::Point2F(l, mid - 4), line, 2.f);
            rt->DrawLine(D2D1::Point2F(l, mid - 4), D2D1::Point2F(l, mid + 12), line, 2.f);
            rt->DrawLine(D2D1::Point2F(l + 8, mid - 12), D2D1::Point2F(l + 22, mid - 12), line, 2.f);
            rt->DrawLine(D2D1::Point2F(rgt, mid - 12), D2D1::Point2F(rgt, mid + 4), line, 2.f);
            rt->DrawLine(D2D1::Point2F(rgt, mid + 4), D2D1::Point2F(rgt - 8, mid + 12), line, 2.f);
            d.accentGlow->SetStartPoint(D2D1::Point2F(l, 0));
            d.accentGlow->SetEndPoint(D2D1::Point2F(rgt, 0));
            rt->FillRectangle(D2D1::RectF(l, mid + 13, rgt, mid + 15), d.accentGlow.Get());
          }
          d.text(label, d.value.Get(), D2D1::RectF(x, y, x + w + 4, y + 36), d.color(selected ? kWhite : kValueGrey, selected ? 1.f : .75f), 1.5f);
          d.hits.push_back({D2D1::RectF(x - 12, y, x + w + 12, y + 36), row.id, i});
          x += w + 34.f;
        }
        break;
      }
      case Kind::Meter:
      {
        const float value = (std::clamp(a.opacity, .1f, 1.f) - .1f) / .9f;
        const int segments = 32;
        const float left = kValuesLeft, right = kValuesRight - 90.f, segW = (right - left) / static_cast<float>(segments);
        for (int i = 0; i < segments; ++i)
        {
          const float sx = left + static_cast<float>(i) * segW;
          const bool lit = (static_cast<float>(i) + .5f) / static_cast<float>(segments) <= value;
          const float t = static_cast<float>(i) / static_cast<float>(segments - 1);
          const UINT32 rgb = lit ? (t < .5f ? 0x2FD8B0 : 0x3FF6E6) : 0x0C2A2A;
          d.shard(sx + 1, mid - 9, segW - 3, 18, 5, d.color(rgb, lit ? .95f : .85f));
        }
        rt->DrawRectangle(D2D1::RectF(left - 4, mid - 13, right + 6, mid + 13), d.color(kCyanLine, .5f), 1.f);
        wchar_t percent[8]{};
        wsprintfW(percent, L"%d%%", static_cast<int>(a.opacity * 100 + .5f));
        d.strokedText(percent, d.tab.Get(), D2D1::RectF(right + 14, y, kValuesRight, y + 36), d.amber.Get(), 1.f, DWRITE_TEXT_ALIGNMENT_TRAILING);
        d.hits.push_back({D2D1::RectF(left - 4, y, right + 6, y + 36), row.id, -3});
        break;
      }
      case Kind::Accent:
      {
        const UINT32 accent = a.accentRgb & 0xFFFFFF;
        d.shard(kValuesLeft, mid - 11, 70, 22, 8, d.color(accent));
        d.strokePolygon({D2D1::Point2F(kValuesLeft + 8, mid - 11), D2D1::Point2F(kValuesLeft + 78, mid - 11), D2D1::Point2F(kValuesLeft + 70, mid + 11), D2D1::Point2F(kValuesLeft, mid + 11)}, d.color(kWhite, .6f), 1.f);
        wchar_t hex[16]{};
        wsprintfW(hex, L"#%06X", accent);
        d.text(hex, d.value.Get(), D2D1::RectF(kValuesLeft + 96, y, kValuesLeft + 200, y + 36), d.color(kWhite), 1.5f);
        d.text(a.dynamicAccent ? L"OVERRIDDEN BY DYNAMIC ACCENT" : L"CLICK TO SELECT", d.small.Get(), D2D1::RectF(kValuesLeft + 210, y, kValuesRight, y + 36), d.color(a.dynamicAccent ? kOrange : kMuted), 1.5f);
        d.hits.push_back({D2D1::RectF(kValuesLeft, y, kValuesRight, y + 36), row.id, -2});
        break;
      }
      case Kind::Shortcut:
      {
        const std::wstring keys = a.activationKeyName ? a.activationKeyName : L"Shift";
        const bool recording = keys == L"Press keys...";
        const float w = d.measure(keys, d.value.Get(), 1.f) + 30.f;
        d.fillPolygon({D2D1::Point2F(kValuesLeft + 8, mid - 12), D2D1::Point2F(kValuesLeft + w, mid - 12), D2D1::Point2F(kValuesLeft + w - 8, mid + 12), D2D1::Point2F(kValuesLeft, mid + 12)}, d.color(recording ? kOrangeDeep : 0x041616, recording ? .45f * pulse + .2f : .85f));
        d.strokePolygon({D2D1::Point2F(kValuesLeft + 8, mid - 12), D2D1::Point2F(kValuesLeft + w, mid - 12), D2D1::Point2F(kValuesLeft + w - 8, mid + 12), D2D1::Point2F(kValuesLeft, mid + 12)}, d.color(recording ? kAmber : kCyan, .9f), 1.5f);
        d.text(recording ? L"PRESS KEYS..." : keys, d.value.Get(), D2D1::RectF(kValuesLeft + 14, y, kValuesLeft + w, y + 36), d.color(recording ? kAmber : kWhite), 1.f);
        d.text(L"DOUBLE-TAP TO TOGGLE INTERACTION", d.small.Get(), D2D1::RectF(kValuesLeft + w + 18, y, kValuesRight, y + 36), d.color(kMuted), 1.5f);
        d.hits.push_back({D2D1::RectF(kValuesLeft, y, kValuesRight, y + 36), row.id, -2});
        break;
      }
      case Kind::Font:
      {
        const std::wstring family = b.fontFamily ? *b.fontFamily : L"Segoe UI Variable Display";
        ComPtr<IDWriteTextFormat> preview;
        d.write->CreateTextFormat(family.c_str(), nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.f, L"en-us", &preview);
        d.text(family, preview ? preview.Get() : d.value.Get(), D2D1::RectF(kValuesLeft, y, kValuesRight - 120, y + 36), d.color(kWhite));
        d.text(L"CHANGE \u25B6", d.small.Get(), D2D1::RectF(kValuesRight - 120, y, kValuesRight, y + 36), d.color(kOrange, pulse), 1.5f, DWRITE_TEXT_ALIGNMENT_TRAILING);
        d.hits.push_back({D2D1::RectF(kValuesLeft, y, kValuesRight, y + 36), row.id, -2});
        break;
      }
      case Kind::Text:
      {
        std::wstring* field = textField(row.id, b);
        const bool editing = d.edit == row.id;
        const float left = kValuesLeft, right = kValuesRight;
        d.fillPolygon({D2D1::Point2F(left + 10, mid - 13), D2D1::Point2F(right, mid - 13), D2D1::Point2F(right - 10, mid + 13), D2D1::Point2F(left, mid + 13)}, d.color(0x020E0E, .9f));
        d.strokePolygon({D2D1::Point2F(left + 10, mid - 13), D2D1::Point2F(right, mid - 13), D2D1::Point2F(right - 10, mid + 13), D2D1::Point2F(left, mid + 13)}, d.color(editing ? kAmber : kTealMid, editing ? 1.f : .8f), editing ? 1.5f : 1.f);
        std::wstring shown = field ? *field : std::wstring{};
        const bool empty = shown.empty();
        if (empty && !editing) shown = row.id == IdArtworkKey ? L"<DEFAULT ICON>" : L"<EMPTY>";
        d.text(shown, d.field.Get(), D2D1::RectF(left + 18, y, right - 16, y + 36), d.color(empty && !editing ? kMuted : kWhite));
        if (editing && static_cast<int>(time * 2.f) % 2 == 0)
        {
          const float caret = left + 18.f + (field ? d.measure(*field, d.field.Get()) : 0.f) + 1.f;
          rt->DrawLine(D2D1::Point2F(caret, mid - 8), D2D1::Point2F(caret, mid + 8), d.color(kAmber), 1.5f);
        }
        d.hits.push_back({D2D1::RectF(left, y, right, y + 36), row.id, -2});
        break;
      }
    }
  }
  if (d.page == 2)
  {
    d.text(L"CLICK A FIELD TO EDIT  //  ENTER TO APPLY  //  ESC TO CANCEL", d.small.Get(), D2D1::RectF(kValuesLeft, kRowsTop + 4 * kRowStep + 8, kValuesRight, kRowsTop + 4 * kRowStep + 30), d.color(kMuted), 1.5f);
  }

  // ---- explanation box --------------------------------------------------------
  const D2D1_POINT_2F box[] = {D2D1::Point2F(24, 626), D2D1::Point2F(560, 626), D2D1::Point2F(576, 642), D2D1::Point2F(576, 722), D2D1::Point2F(24, 722)};
  if (auto path = d.polygon(box, 5))
  {
    rt->FillGeometry(path.Get(), d.color(0x010808, .88f));
    rt->DrawGeometry(path.Get(), d.color(kTealMid, .7f), 1.f);
  }
  d.glowLine(24, 626, 200, kCyan, .8f);
  const wchar_t* explanation = L"Hover over an option to see what it does.";
  for (int r = 0; r < page.count; ++r) if (page.rows[r].id == d.hover) explanation = page.rows[r].help;
  d.text(explanation, d.help.Get(), D2D1::RectF(40, 636, 560, 716), d.color(0xC8DADA), 0.f, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

  // ---- buttons ----------------------------------------------------------------
  auto button = [&](float x, float w, const wchar_t* label, bool primary, int id)
  {
    const float top = 650.f, bottom = 698.f;
    const D2D1_POINT_2F plate[] = {D2D1::Point2F(x + 14, top), D2D1::Point2F(x + w, top), D2D1::Point2F(x + w - 14, bottom), D2D1::Point2F(x, bottom)};
    auto path = d.polygon(plate, 4);
    if (!path) return;
    if (primary)
    {
      for (int i = 3; i >= 1; --i)
      {
        const float g = static_cast<float>(i) * 2.5f;
        d.fillPolygon({D2D1::Point2F(x + 14 - g, top - g), D2D1::Point2F(x + w + g, top - g), D2D1::Point2F(x + w - 14 + g, bottom + g), D2D1::Point2F(x - g, bottom + g)}, d.color(kAmber, .06f * pulse));
      }
      d.amber->SetStartPoint(D2D1::Point2F(0, top));
      d.amber->SetEndPoint(D2D1::Point2F(0, bottom));
      rt->FillGeometry(path.Get(), d.amber.Get());
      d.text(label, d.button.Get(), D2D1::RectF(x, top, x + w, bottom), d.color(0x1A0A00), 3.f, DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    else
    {
      rt->FillGeometry(path.Get(), d.color(0x041616, .9f));
      rt->DrawGeometry(path.Get(), d.color(kCyan, .9f), 1.5f);
      d.strokedText(label, d.button.Get(), D2D1::RectF(x, top, x + w, bottom), d.color(kCyan), 3.f, DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    d.hits.push_back({D2D1::RectF(x, top, x + w, bottom), id, -1});
  };
  button(600, 126, L"APPLY", true, -200);
  button(738, 126, L"CLOSE", false, -201);

  // ---- footer -----------------------------------------------------------------
  d.shard(24, 734, 120, 6, 6, d.color(kOrange, .65f));
  d.shard(152, 734, 60, 6, 6, d.color(kAmber, .5f));
  d.glowPill(600, 734, 90, 6, kAmber, pulse);
  d.text(L"NMF  //  SYSTEM CONFIGURATION", d.caption.Get(), D2D1::RectF(226, 728, 560, 746), d.color(kMuted), 2.f);
  d.text(L"CSLA-INSPIRED DIRECT2D SURFACE", d.caption.Get(), D2D1::RectF(600, 742, 862, 758), d.color(kTealMid), 1.5f, DWRITE_TEXT_ALIGNMENT_TRAILING);

  if (rt->EndDraw() == D2DERR_RECREATE_TARGET) d.releaseDeviceResources();
  EndPaint(hwnd_, &ps);
}

void SettingsWindow::hover(float x, float y)
{
  auto& d = *impl_;
  int id = -1;
  for (const auto& h : d.hits)
    if (h.id >= 0 && x >= h.rect.left && x < h.rect.right && y >= h.rect.top && y < h.rect.bottom) { id = h.id; break; }
  if (id != d.hover) { d.hover = id; refresh(); }
}

void SettingsWindow::drag(float x)
{
  const float left = kValuesLeft, right = kValuesRight - 90.f;
  const float t = std::clamp((x - left) / (right - left), 0.f, 1.f);
  bindings_.appearance->opacity = .1f + .9f * t;
  apply();
}

void SettingsWindow::click(float x, float y)
{
  auto& d = *impl_;
  // Prefer the most specific region: hits are added row-first, choices after.
  const Hit* best = nullptr;
  for (const auto& h : d.hits)
    if (x >= h.rect.left && x < h.rect.right && y >= h.rect.top && y < h.rect.bottom && (!best || h.choice != -1)) best = &h;
  if (!best) return;
  if (best->id <= -100 && best->id > -200) { d.page = -100 - best->id; d.edit = -1; d.hover = -1; d.anims.clear(); refresh(); return; }
  if (best->id == -200) { apply(); return; }
  if (best->id == -201) { ShowWindow(hwnd_, SW_HIDE); KillTimer(hwnd_, kAnimationTimer); return; }

  const int id = best->id;
  if (best->choice >= 0) { selectChoice(id, best->choice, bindings_); d.edit = -1; apply(); return; }
  switch (id)
  {
    case IdOpacity: d.dragging = true; SetCapture(hwnd_); drag(x); return;
    case IdRenderer:
      if (bindings_.cycleRenderer) { bindings_.cycleRenderer(bindings_.context); apply(); }
      return;
    case IdAccent: chooseAccent(); return;
    case IdShortcut: if (bindings_.recordShortcut) bindings_.recordShortcut(bindings_.context); refresh(); return;
    case IdFont: chooseFont(); return;
    case IdAppId: case IdIconKey: case IdArtworkKey: d.edit = id; SetFocus(hwnd_); refresh(); return;
    default:
      // Clicking a toggle row outside its choices flips it, like pressing Start on a row.
      {
        const Choices c = choicesFor(id, bindings_);
        if (c.count == 2) { selectChoice(id, c.current == 1 ? 0 : 1, bindings_); apply(); }
      }
      return;
  }
}

LRESULT CALLBACK SettingsWindow::windowProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
  auto* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(h, GWLP_USERDATA));
  if (m == WM_NCCREATE)
  {
    self = static_cast<SettingsWindow*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
    self->hwnd_ = h;
    SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  return self ? self->handleMessage(m, w, l) : DefWindowProcW(h, m, w, l);
}

LRESULT SettingsWindow::handleMessage(UINT m, WPARAM w, LPARAM l)
{
  auto& d = *impl_;
  const float scale = 96.f / d.dpi;
  const float mx = static_cast<float>(GET_X_LPARAM(l)) * scale, my = static_cast<float>(GET_Y_LPARAM(l)) * scale;
  switch (m)
  {
    case WM_PAINT: paint(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_TIMER: if (w == kAnimationTimer) InvalidateRect(hwnd_, nullptr, FALSE); return 0;
    case WM_SIZE: if (d.target) d.target->Resize(D2D1::SizeU(LOWORD(l), HIWORD(l))); refresh(); return 0;
    case WM_DPICHANGED:
    {
      d.dpi = static_cast<float>(HIWORD(w));
      const auto* suggested = reinterpret_cast<const RECT*>(l);
      SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top, suggested->right - suggested->left, suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
      if (d.target) d.target->SetDpi(d.dpi, d.dpi);
      refresh();
      return 0;
    }
    case WM_MOUSEMOVE:
      if (!d.tracking) { TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, hwnd_, 0}; d.tracking = TrackMouseEvent(&track) != FALSE; }
      if (d.dragging) drag(mx); else hover(mx, my);
      return 0;
    case WM_MOUSELEAVE: d.tracking = false; if (!d.dragging) { d.hover = -1; refresh(); } return 0;
    case WM_LBUTTONDOWN: click(mx, my); return 0;
    case WM_LBUTTONUP: if (d.dragging) { d.dragging = false; ReleaseCapture(); } return 0;
    case WM_CHAR:
      if (d.edit >= 0)
      {
        std::wstring* value = textField(d.edit, bindings_);
        if (!value) return 0;
        if (w == VK_BACK) { if (!value->empty()) value->pop_back(); }
        else if (w == VK_RETURN) { d.edit = -1; apply(); }
        else if (w >= 32 && value->size() < 127 && (d.edit != IdAppId || (w >= L'0' && w <= L'9'))) value->push_back(static_cast<wchar_t>(w));
        refresh();
        return 0;
      }
      break;
    case WM_KEYDOWN: if (w == VK_ESCAPE) { d.edit = -1; refresh(); return 0; } break;
    case WM_CLOSE: ShowWindow(hwnd_, SW_HIDE); KillTimer(hwnd_, kAnimationTimer); return 0;
    case WM_DESTROY: KillTimer(hwnd_, kAnimationTimer); d.releaseDeviceResources(); hwnd_ = nullptr; return 0;
  }
  return DefWindowProcW(hwnd_, m, w, l);
}
}
