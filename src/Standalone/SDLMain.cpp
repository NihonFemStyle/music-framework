// SDL3 frontend for Nihon's Music Framework.
//
// Feature parity with the native DirectX application: every overlay mode and
// setting, media controls, the interaction shortcut, Windowed Stream Mode,
// Overkill spectra, Discord Rich Presence, the settings window, and a
// notification-area menu. The overlay is drawn by the shared Direct2D painter
// into a CPU buffer (mo::SoftwareRenderer) and presented through whichever SDL
// renderer is selected, so the output is identical on every SDL backend.
#include <windows.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <MusicOverlay/Core/OverlayState.h>
#include <MusicOverlay/Renderer/SoftwareRenderer.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include <winrt/base.h>

#include "Platform/Windows/DiscordRpc.h"
#include "Platform/Windows/MediaSessionManager.h"
#include "Platform/Windows/ProcessSpectrumCapture.h"
#include "SettingsWindow.h"
#include "StandaloneSettings.h"
#include "resource.h"

#include <commdlg.h>

namespace
{
constexpr int kNormalWidth = 430, kNormalHeight = 112, kCompactWidth = 330, kCompactHeight = 76, kOverkillWidth = 520, kOverkillHeight = 180;
constexpr ULONGLONG kInteractionDoubleTapMs = 450;
constexpr std::array<std::uint32_t, 5> kBarCounts{16, 24, 32, 48, 64};
constexpr std::array<std::uint32_t, 5> kSpectrumDelays{0, 50, 100, 200, 350};
constexpr std::array<std::uint32_t, 4> kSpectrumHeights{25, 50, 75, 100};
constexpr std::array<std::uint32_t, 5> kSpectrumFades{0, 5, 10, 20, 30};
constexpr std::array<std::uint32_t, 4> kHueSpeeds{15, 30, 60, 120};

mo::standalone::StandaloneSettings g_settings;
mo::Appearance& g_appearance = g_settings.appearance;
mo::OverlayState g_state;
mo::win::MediaSessionManager g_media(g_state);
mo::win::ProcessSpectrumCapture g_spectrum;
mo::win::DiscordRpc g_discordRpc;
mo::standalone::SettingsWindow g_settingsWindow;
mo::SoftwareRenderer g_painter;

SDL_Window* g_window{};
SDL_Renderer* g_renderer{};
SDL_Texture* g_texture{};
int g_textureWidth{}, g_textureHeight{};
HWND g_hwnd{};
bool g_running = true;

std::wstring g_comboName = L"Shift";
std::vector<int> g_recorded;
bool g_interactive{}, g_comboWasDown{}, g_recording{}, g_appliedWindowedMode{}, g_modeApplied{};
bool g_scanWasDown[256]{};
ULONGLONG g_lastTap{}, g_lastMediaPoll{}, g_lastSpectrumCheck{};
const std::uint8_t* g_bannerData{};
std::uint32_t g_bannerSize{};

std::string g_activeRenderer;
std::vector<std::string> g_rendererChoices; // "auto" plus every driver SDL reports
std::wstring g_rendererLabel;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
std::string utf8(const std::wstring& text)
{
  if (text.empty()) return {};
  const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  std::string result(static_cast<std::size_t>(std::max(size, 0)), '\0');
  if (size) WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
  return result;
}
std::wstring widen(std::string_view text)
{
  if (text.empty()) return {};
  const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  std::wstring result(static_cast<std::size_t>(std::max(size, 0)), L'\0');
  if (size) MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
  return result;
}

const char* friendlyRendererName(std::string_view driver)
{
  if (driver == "auto") return "Automatic (SDL default)";
  if (driver == "direct3d") return "Direct3D 9";
  if (driver == "direct3d11") return "Direct3D 11";
  if (driver == "direct3d12") return "Direct3D 12";
  if (driver == "opengl") return "OpenGL";
  if (driver == "opengles2") return "OpenGL ES 2";
  if (driver == "vulkan") return "Vulkan";
  if (driver == "gpu") return "SDL GPU";
  if (driver == "software") return "Software";
  return nullptr;
}
std::string rendererDisplay(const std::string& driver)
{
  const char* name = friendlyRendererName(driver);
  return name ? name : driver;
}

void saveSettings() { mo::standalone::saveSettings(g_settings); }

std::wstring keyName(int vk)
{
  wchar_t name[64]{};
  UINT scan = MapVirtualKeyW(static_cast<UINT>(vk), MAPVK_VK_TO_VSC) << 16;
  if (vk == VK_LEFT || vk == VK_UP || vk == VK_RIGHT || vk == VK_DOWN || vk == VK_RCONTROL || vk == VK_RMENU) scan |= 1 << 24;
  if (GetKeyNameTextW(static_cast<LONG>(scan), name, 64) > 0) return name;
  wchar_t fallback[16]{};
  wsprintfW(fallback, L"VK %02X", vk);
  return fallback;
}
void updateComboName()
{
  g_comboName.clear();
  for (std::size_t i = 0; i < g_settings.shortcut.size(); ++i) { if (i) g_comboName += L" + "; g_comboName += keyName(g_settings.shortcut[i]); }
  g_appearance.activationKeyName = g_recording ? L"Press keys..." : g_comboName.c_str();
}
bool comboDown()
{
  for (int vk : g_settings.shortcut) if (!(GetAsyncKeyState(vk) & 0x8000)) return false;
  return !g_settings.shortcut.empty();
}
void updateCornerName()
{
  static const wchar_t* names[]{L"Top left", L"Top right", L"Bottom left", L"Bottom right"};
  g_appearance.cornerName = names[std::min<std::uint32_t>(g_appearance.corner, 3)];
}
void updateRendererLabel()
{
  g_rendererLabel = widen(rendererDisplay(utf8(g_settings.sdlRenderer)));
  if (!g_rendererLabel.empty()) CharUpperBuffW(g_rendererLabel.data(), static_cast<DWORD>(g_rendererLabel.size()));
  g_appearance.backendName = L"SDL";
}

// ---------------------------------------------------------------------------
// Window behavior (mirrors the native host)
// ---------------------------------------------------------------------------
int collapsedHeight() { return g_appearance.overkill ? kOverkillHeight : (g_appearance.compact ? kCompactHeight : kNormalHeight); }
int collapsedWidth() { return g_appearance.overkill ? kOverkillWidth : (g_appearance.compact ? kCompactWidth : kNormalWidth); }

void constrainToMonitor()
{
  RECT window{};
  if (!g_hwnd || !GetWindowRect(g_hwnd, &window)) return;
  HMONITOR monitor = MonitorFromRect(&window, MONITOR_DEFAULTTONEAREST);
  MONITORINFO info{sizeof(info)};
  if (!GetMonitorInfoW(monitor, &info)) return;
  const LONG width = window.right - window.left, height = window.bottom - window.top;
  LONG x = width >= info.rcWork.right - info.rcWork.left ? info.rcWork.left : std::clamp(window.left, info.rcWork.left, info.rcWork.right - width);
  LONG y = height >= info.rcWork.bottom - info.rcWork.top ? info.rcWork.top : std::clamp(window.top, info.rcWork.top, info.rcWork.bottom - height);
  if (x != window.left || y != window.top) SetWindowPos(g_hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void positionOverkill()
{
  if (!g_hwnd || g_settings.windowedMode || !g_appearance.overkill || g_appearance.settingsOpen) return;
  HMONITOR monitor = MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST);
  MONITORINFO info{sizeof(info)};
  GetMonitorInfoW(monitor, &info);
  RECT bounds = info.rcWork;
  HWND foreground = GetForegroundWindow();
  if (foreground && foreground != g_hwnd)
  {
    RECT window{};
    GetWindowRect(foreground, &window);
    HMONITOR foregroundMonitor = MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST);
    MONITORINFO foregroundInfo{sizeof(foregroundInfo)};
    GetMonitorInfoW(foregroundMonitor, &foregroundInfo);
    if (abs(window.left - foregroundInfo.rcMonitor.left) <= 2 && abs(window.top - foregroundInfo.rcMonitor.top) <= 2 &&
        abs(window.right - foregroundInfo.rcMonitor.right) <= 2 && abs(window.bottom - foregroundInfo.rcMonitor.bottom) <= 2)
      bounds = foregroundInfo.rcMonitor;
  }
  const int x = (g_appearance.corner == 0 || g_appearance.corner == 2) ? bounds.left : bounds.right - kOverkillWidth;
  const int y = g_appearance.corner < 2 ? bounds.top : bounds.bottom - kOverkillHeight;
  SetWindowPos(g_hwnd, HWND_TOPMOST, x, y, kOverkillWidth, kOverkillHeight, SWP_NOACTIVATE);
}

void resizeWindow()
{
  if (!g_hwnd || g_settings.windowedMode) return;
  RECT r{};
  GetWindowRect(g_hwnd, &r);
  SetWindowPos(g_hwnd, HWND_TOPMOST, r.left, r.top, collapsedWidth(), collapsedHeight(), SWP_NOACTIVATE);
  if (g_appearance.overkill) positionOverkill(); else constrainToMonitor();
}

void applyInteraction()
{
  if (!g_hwnd) return;
  const bool interactive = g_settings.windowedMode || g_interactive;
  const LONG_PTR style = GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);
  LONG_PTR wanted = style;
  if (g_settings.windowedMode) wanted &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT | WS_EX_LAYERED);
  else
  {
    wanted |= WS_EX_LAYERED;
    if (interactive) wanted &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT); else wanted |= WS_EX_TRANSPARENT;
  }
  if (wanted != style)
  {
    SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, wanted);
    if (!g_settings.windowedMode) SetLayeredWindowAttributes(g_hwnd, 0, 255, LWA_ALPHA);
    SetWindowPos(g_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  }
  g_appearance.interactive = interactive;
}

void applyWindowMode()
{
  if (!g_hwnd) return;
  const bool modeChanged = !g_modeApplied || g_appliedWindowedMode != g_settings.windowedMode;
  if (g_appliedWindowedMode && !g_settings.windowedMode) g_interactive = false;
  RECT r{};
  GetWindowRect(g_hwnd, &r);
  // Taskbar presence only updates when the window is re-shown after the style change.
  if (modeChanged && g_modeApplied) ShowWindow(g_hwnd, SW_HIDE);
  if (g_settings.windowedMode)
  {
    g_interactive = true;
    SDL_SetWindowAlwaysOnTop(g_window, false);
    SDL_SetWindowBordered(g_window, true);
    SDL_SetWindowResizable(g_window, true);
    SDL_SetWindowFocusable(g_window, true);
    LONG_PTR ex = GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);
    ex &= ~static_cast<LONG_PTR>(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST);
    ex |= WS_EX_APPWINDOW;
    SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, ex);
    const int width = std::max<LONG>(640, r.right - r.left), height = std::max<LONG>(240, r.bottom - r.top);
    SetWindowPos(g_hwnd, HWND_NOTOPMOST, r.left, r.top, width, height, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
  }
  else
  {
    SDL_SetWindowBordered(g_window, false);
    SDL_SetWindowResizable(g_window, false);
    SDL_SetWindowFocusable(g_window, false);
    SDL_SetWindowAlwaysOnTop(g_window, true);
    LONG_PTR ex = GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);
    ex &= ~static_cast<LONG_PTR>(WS_EX_APPWINDOW | WS_EX_TRANSPARENT);
    ex |= WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TOPMOST | (g_interactive ? 0 : WS_EX_TRANSPARENT);
    SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, ex);
    SetLayeredWindowAttributes(g_hwnd, 0, 255, LWA_ALPHA);
    SetWindowPos(g_hwnd, HWND_TOPMOST, r.left, r.top, collapsedWidth(), collapsedHeight(), SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (g_appearance.overkill) positionOverkill(); else constrainToMonitor();
  }
  applyInteraction();
  g_appliedWindowedMode = g_settings.windowedMode;
  g_modeApplied = true;
  saveSettings();
}

// ---------------------------------------------------------------------------
// Notification-area menu (SDL tray API)
// ---------------------------------------------------------------------------
SDL_Tray* g_tray{};
SDL_TrayEntry* g_interactionEntry{};
struct TrayState { SDL_TrayEntry* entry; std::function<bool()> checked; std::function<bool()> enabled; };
std::vector<TrayState> g_trayStates;
std::deque<std::function<void()>> g_trayActions; // deque keeps addresses stable for SDL userdata

void refreshTray()
{
  for (auto& item : g_trayStates)
  {
    if (item.checked) SDL_SetTrayEntryChecked(item.entry, item.checked());
    if (item.enabled) SDL_SetTrayEntryEnabled(item.entry, item.enabled());
  }
  if (g_interactionEntry) SDL_SetTrayEntryLabel(g_interactionEntry, g_interactive ? "Disable interaction" : "Enable interaction");
}

void afterSettingChanged()
{
  saveSettings();
  g_settingsWindow.refresh();
  refreshTray();
}

void SDLCALL onTrayEntry(void* userdata, SDL_TrayEntry*)
{
  (*static_cast<std::function<void()>*>(userdata))();
  afterSettingChanged();
}

SDL_TrayEntry* addEntry(SDL_TrayMenu* menu, const std::string& label, SDL_TrayEntryFlags flags, std::function<void()> action,
                        std::function<bool()> checked = {}, std::function<bool()> enabled = {})
{
  SDL_TrayEntry* entry = SDL_InsertTrayEntryAt(menu, -1, label.c_str(), flags);
  if (!entry) return nullptr;
  g_trayActions.push_back(std::move(action));
  SDL_SetTrayEntryCallback(entry, onTrayEntry, &g_trayActions.back());
  if (checked || enabled) g_trayStates.push_back({entry, std::move(checked), std::move(enabled)});
  return entry;
}
SDL_TrayEntry* addButton(SDL_TrayMenu* menu, const std::string& label, std::function<void()> action, std::function<bool()> enabled = {})
{
  return addEntry(menu, label, SDL_TRAYENTRY_BUTTON, std::move(action), {}, std::move(enabled));
}
void addCheck(SDL_TrayMenu* menu, const std::string& label, std::function<bool()> checked, std::function<void()> action)
{
  const SDL_TrayEntryFlags flags = SDL_TRAYENTRY_CHECKBOX | (checked() ? SDL_TRAYENTRY_CHECKED : 0u);
  addEntry(menu, label, flags, std::move(action), std::move(checked));
}
void addToggle(SDL_TrayMenu* menu, const std::string& label, bool* value, std::function<void()> after = {})
{
  addCheck(menu, label, [value] { return *value; }, [value, after] { *value = !*value; if (after) after(); });
}
SDL_TrayMenu* addSubmenu(SDL_TrayMenu* menu, const std::string& label)
{
  SDL_TrayEntry* entry = SDL_InsertTrayEntryAt(menu, -1, label.c_str(), SDL_TRAYENTRY_SUBMENU);
  return entry ? SDL_CreateTraySubmenu(entry) : nullptr;
}
void addSeparator(SDL_TrayMenu* menu) { SDL_InsertTrayEntryAt(menu, -1, nullptr, SDL_TRAYENTRY_BUTTON); }
template <std::size_t N>
void addChoices(SDL_TrayMenu* menu, const std::array<std::uint32_t, N>& values, std::uint32_t* target, const std::wstring& suffix, std::function<void()> after = {})
{
  if (!menu) return;
  for (std::uint32_t value : values)
    addCheck(menu, utf8(std::to_wstring(value) + suffix), [target, value] { return *target == value; }, [target, value, after] { *target = value; if (after) after(); });
}

void chooseAccent(HWND owner)
{
  CHOOSECOLORW cc{sizeof(cc)};
  static COLORREF custom[16]{};
  cc.hwndOwner = owner;
  cc.rgbResult = RGB((g_appearance.accentRgb >> 16) & 255, (g_appearance.accentRgb >> 8) & 255, g_appearance.accentRgb & 255);
  cc.lpCustColors = custom;
  cc.Flags = CC_FULLOPEN | CC_RGBINIT;
  if (ChooseColorW(&cc)) g_appearance.accentRgb = (GetRValue(cc.rgbResult) << 16) | (GetGValue(cc.rgbResult) << 8) | GetBValue(cc.rgbResult);
}

void startRecordingShortcut()
{
  g_recording = true;
  g_recorded.clear();
  ZeroMemory(g_scanWasDown, sizeof(g_scanWasDown));
  updateComboName();
  g_settingsWindow.refreshShortcut();
}

void selectRenderer(const std::string& driver)
{
  g_settings.sdlRenderer = widen(driver);
  updateRendererLabel();
  if (driver != g_activeRenderer)
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Nihon's Music Framework", ("The " + rendererDisplay(driver) + " renderer will be used after restarting the SDL frontend.").c_str(), g_window);
}

SDL_Surface* surfaceFromIcon(HICON icon)
{
  ICONINFO info{};
  if (!icon || !GetIconInfo(icon, &info)) return nullptr;
  SDL_Surface* surface = nullptr;
  BITMAP bitmap{};
  if (info.hbmColor && GetObjectW(info.hbmColor, sizeof(bitmap), &bitmap) && bitmap.bmWidth > 0 && bitmap.bmHeight > 0)
  {
    surface = SDL_CreateSurface(bitmap.bmWidth, bitmap.bmHeight, SDL_PIXELFORMAT_ARGB8888);
    if (surface)
    {
      BITMAPINFO bi{};
      bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bi.bmiHeader.biWidth = bitmap.bmWidth;
      bi.bmiHeader.biHeight = -bitmap.bmHeight;
      bi.bmiHeader.biPlanes = 1;
      bi.bmiHeader.biBitCount = 32;
      bi.bmiHeader.biCompression = BI_RGB;
      HDC dc = GetDC(nullptr);
      if (!GetDIBits(dc, info.hbmColor, 0, static_cast<UINT>(bitmap.bmHeight), surface->pixels, &bi, DIB_RGB_COLORS)) { SDL_DestroySurface(surface); surface = nullptr; }
      ReleaseDC(nullptr, dc);
    }
  }
  if (info.hbmColor) DeleteObject(info.hbmColor);
  if (info.hbmMask) DeleteObject(info.hbmMask);
  return surface;
}

void buildTray(HICON icon)
{
  SDL_Surface* surface = surfaceFromIcon(icon);
  g_tray = SDL_CreateTray(surface, "Nihon's Music Framework (SDL)");
  if (surface) SDL_DestroySurface(surface);
  if (!g_tray) return;
  SDL_TrayMenu* menu = SDL_CreateTrayMenu(g_tray);
  if (!menu) return;

  SYSTEMTIME localTime{};
  GetLocalTime(&localTime);
  const bool aprilFools = localTime.wMonth == 4 && localTime.wDay == 1;

  addButton(menu, "Open settings", [] { g_settingsWindow.show(g_hwnd); });
  addToggle(menu, "Windowed mode", &g_settings.windowedMode, [] { applyWindowMode(); });
  g_interactionEntry = addButton(menu, "Enable interaction", [] { if (!g_settings.windowedMode) { g_interactive = !g_interactive; applyInteraction(); } }, [] { return !g_settings.windowedMode; });
  addSeparator(menu);

  if (SDL_TrayMenu* opacity = addSubmenu(menu, "Transparency"))
    for (int value : {10, 25, 50, 75, 100})
      addCheck(opacity, std::to_string(value) + "%", [value] { return static_cast<int>(g_appearance.opacity * 100 + .5f) == value; }, [value] { g_appearance.opacity = static_cast<float>(value) / 100.f; });
  addToggle(menu, "Compact mode", &g_appearance.compact, [] { resizeWindow(); });
  addButton(menu, "Choose accent...", [] { chooseAccent(g_hwnd); });
  addButton(menu, "Choose text font...", [] { g_settingsWindow.chooseFont(); });
  addToggle(menu, "Dynamic accent", &g_appearance.dynamicAccent);
  addToggle(menu, "Artwork background", &g_appearance.artworkBackground);
  addButton(menu, "Set interaction shortcut...", [] { g_settingsWindow.show(g_hwnd); startRecordingShortcut(); });
  addToggle(menu, "Ignore title parentheses", &g_appearance.ignoreParenthetical);
  if (SDL_TrayMenu* renderers = addSubmenu(menu, "SDL renderer (restart required)"))
    for (const auto& driver : g_rendererChoices)
      addCheck(renderers, rendererDisplay(driver), [driver] { return utf8(g_settings.sdlRenderer) == driver; }, [driver] { selectRenderer(driver); });
  addSeparator(menu);

  addToggle(menu, "Overkill mode", &g_appearance.overkill, [] { resizeWindow(); });
  if (SDL_TrayMenu* corner = addSubmenu(menu, "Overkill corner"))
  {
    const char* corners[]{"Top left", "Top right", "Bottom left", "Bottom right"};
    for (std::uint32_t i = 0; i < 4; ++i)
      addCheck(corner, corners[i], [i] { return g_appearance.corner == i; }, [i] { g_appearance.corner = i; updateCornerName(); if (g_appearance.overkill && !g_appearance.settingsOpen) positionOverkill(); });
  }
  addChoices(addSubmenu(menu, "Spectrum bars / points"), kBarCounts, &g_appearance.spectrumBars, L"");
  addChoices(addSubmenu(menu, "Spectrum delay"), kSpectrumDelays, &g_appearance.spectrumDelayMs, L" ms");
  if (SDL_TrayMenu* response = addSubmenu(menu, "Spectrum response"))
  {
    const char* names[]{"Fast", "Balanced", "Smooth"};
    for (std::uint32_t i = 0; i < 3; ++i)
      addCheck(response, names[i], [i] { return g_appearance.spectrumResponse == i; }, [i] { g_appearance.spectrumResponse = i; g_spectrum.setResponse(i); });
  }
  addChoices(addSubmenu(menu, "Spectrum max height"), kSpectrumHeights, &g_appearance.spectrumMaxHeight, L"%");
  addChoices(addSubmenu(menu, "Spectrum top fade"), kSpectrumFades, &g_appearance.spectrumFade, L"%");
  addToggle(menu, "Curves style", &g_appearance.spectrumCurves);
  addToggle(menu, "Chroma spectrum", &g_appearance.spectrumChroma);
  addChoices(addSubmenu(menu, "Chroma hue shift"), kHueSpeeds, &g_appearance.spectrumHueSpeed, aprilFools ? L"\u00C2\u00B0/s" : L"\u00B0/s");
  addToggle(menu, "Artwork spectrum", &g_appearance.artworkSpectrum);
  addToggle(menu, "Show banner", &g_appearance.showBanner);
  addSeparator(menu);
  addToggle(menu, "Discord Rich Presence", &g_settings.discord.enabled, [] { if (!g_settings.discord.enabled) g_discordRpc.disconnect(); });
  addSeparator(menu);
  addButton(menu, "Exit", [] { g_running = false; });
  refreshTray();
}

// ---------------------------------------------------------------------------
// Settings window bindings
// ---------------------------------------------------------------------------
void settingsApply(void*)
{
  updateCornerName();
  g_spectrum.setResponse(g_appearance.spectrumResponse);
  if (!g_settings.discord.enabled) g_discordRpc.disconnect();
  applyWindowMode();
  resizeWindow();
  saveSettings();
  refreshTray();
}
void settingsRecordShortcut(void*) { startRecordingShortcut(); }
const wchar_t* settingsRendererName(void*) { return g_rendererLabel.c_str(); }
void settingsCycleRenderer(void*)
{
  if (g_rendererChoices.empty()) return;
  const std::string current = utf8(g_settings.sdlRenderer);
  auto it = std::find(g_rendererChoices.begin(), g_rendererChoices.end(), current);
  const std::string next = (it == g_rendererChoices.end() || ++it == g_rendererChoices.end()) ? g_rendererChoices.front() : *it;
  g_settings.sdlRenderer = widen(next);
  updateRendererLabel();
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
void scanRecording()
{
  bool any = false;
  for (int vk = 8; vk < 255; ++vk)
  {
    if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON) continue;
    const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
    if (down && !g_scanWasDown[vk] && std::find(g_recorded.begin(), g_recorded.end(), vk) == g_recorded.end()) g_recorded.push_back(vk);
    g_scanWasDown[vk] = down;
    if (down) any = true;
  }
  if (!any && !g_recorded.empty())
  {
    g_settings.shortcut = g_recorded;
    g_recorded.clear();
    g_recording = false;
    updateComboName();
    g_settingsWindow.refreshShortcut();
    saveSettings();
  }
}

void scanToggle()
{
  if (g_settings.windowedMode) return;
  const bool down = comboDown();
  if (down && !g_comboWasDown)
  {
    const ULONGLONG now = GetTickCount64();
    if (g_lastTap && now - g_lastTap <= kInteractionDoubleTapMs) { g_interactive = !g_interactive; g_lastTap = 0; applyInteraction(); refreshTray(); }
    else g_lastTap = now;
  }
  g_comboWasDown = down;
  if (g_lastTap && GetTickCount64() - g_lastTap > kInteractionDoubleTapMs) g_lastTap = 0;
}

bool handleMediaControlClick(int x, int y)
{
  if (!g_appearance.interactive || !g_hwnd) return false;
  RECT client{};
  GetClientRect(g_hwnd, &client);
  constexpr int button = 28, gap = 5, count = 5;
  constexpr int total = button * count + gap * (count - 1);
  const int left = std::max(8, static_cast<int>(client.right) - total - 14);
  const int top = std::max(8, static_cast<int>(client.bottom) - 38);
  if (y < top || y >= top + button || x < left || x >= left + total) return false;
  const int stride = button + gap, offset = x - left;
  if (offset % stride >= button) return true;
  const auto track = g_state.snapshot();
  switch (offset / stride)
  {
    case 0: if (track.canPrevious) g_media.previous(); break;
    case 1: if (track.canPlayPause) g_media.togglePlayPause(); break;
    case 2: if (track.canNext) g_media.next(); break;
    case 3: if (track.canShuffle) g_media.toggleShuffle(); break;
    case 4: if (track.canRepeat) g_media.cycleRepeat(); break;
    default: return false;
  }
  return true;
}

void handleMouseDown(float windowX, float windowY)
{
  // SDL reports window coordinates; the painter and hit geometry use pixels.
  int windowWidth{}, windowHeight{}, pixelWidth{}, pixelHeight{};
  SDL_GetWindowSize(g_window, &windowWidth, &windowHeight);
  SDL_GetWindowSizeInPixels(g_window, &pixelWidth, &pixelHeight);
  const float scaleX = windowWidth ? static_cast<float>(pixelWidth) / static_cast<float>(windowWidth) : 1.f;
  const float scaleY = windowHeight ? static_cast<float>(pixelHeight) / static_cast<float>(windowHeight) : 1.f;
  const int x = static_cast<int>(windowX * scaleX), y = static_cast<int>(windowY * scaleY);
  if (handleMediaControlClick(x, y)) return;
  if (g_settings.windowedMode || (g_interactive && !g_appearance.overkill))
  {
    if (g_settings.windowedMode) SetForegroundWindow(g_hwnd);
    ReleaseCapture();
    SendMessageW(g_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
  }
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------
void tick()
{
  if (g_recording) scanRecording(); else scanToggle();
  const ULONGLONG now = GetTickCount64();
  if (!g_lastMediaPoll || now - g_lastMediaPoll >= 250)
  {
    g_lastMediaPoll = now;
    g_media.poll();
    g_discordRpc.update(g_state.snapshot(), g_settings.discord);
  }
  if (g_appearance.overkill && now - g_lastSpectrumCheck >= 1000)
  {
    g_lastSpectrumCheck = now;
    g_spectrum.ensureForApp(g_state.snapshot().sourceAppId);
    positionOverkill();
  }
  else if (!g_appearance.overkill && !g_spectrum.activeAppId().empty()) g_spectrum.stop();
}

void render()
{
  int width{}, height{};
  SDL_GetWindowSizeInPixels(g_window, &width, &height);
  if (width <= 0 || height <= 0) return;
  g_appearance.logoData = g_bannerData;
  g_appearance.logoDataSize = g_bannerSize;
  mo::FrameView frame{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 96.f, g_state.snapshot(), g_appearance, g_spectrum.snapshot()};
  SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 0);
  SDL_RenderClear(g_renderer);
  if (g_painter.render(frame))
  {
    if (!g_texture || g_textureWidth != width || g_textureHeight != height)
    {
      if (g_texture) SDL_DestroyTexture(g_texture);
      g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
      g_textureWidth = width; g_textureHeight = height;
      if (g_texture)
      {
        SDL_SetTextureBlendMode(g_texture, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
        SDL_SetTextureScaleMode(g_texture, SDL_SCALEMODE_NEAREST);
      }
    }
    if (g_texture && SDL_UpdateTexture(g_texture, nullptr, g_painter.pixels(), static_cast<int>(g_painter.stride())))
    {
      // Premultiplied content: overall transparency scales color and alpha together.
      const float opacity = std::clamp(g_appearance.opacity, .1f, 1.f);
      SDL_SetTextureColorModFloat(g_texture, opacity, opacity, opacity);
      SDL_SetTextureAlphaModFloat(g_texture, opacity);
      SDL_RenderTexture(g_renderer, g_texture, nullptr, nullptr);
    }
  }
  SDL_RenderPresent(g_renderer);
}

bool createRenderer(const std::string& requested)
{
  if (requested != "auto")
  {
    g_renderer = SDL_CreateRenderer(g_window, requested.c_str());
    if (!g_renderer)
    {
      const std::string message = "The " + rendererDisplay(requested) + " renderer could not be started (" + SDL_GetError() + "). The SDL default renderer will be used instead.";
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Nihon's Music Framework", message.c_str(), nullptr);
    }
  }
  if (!g_renderer) g_renderer = SDL_CreateRenderer(g_window, nullptr);
  if (!g_renderer) return false;
  const char* actual = SDL_GetRendererName(g_renderer);
  g_activeRenderer = actual ? actual : "auto";
  SDL_SetRenderVSync(g_renderer, 1);
  return true;
}
}

int main(int argc, char** argv)
{
  winrt::init_apartment(winrt::apartment_type::multi_threaded);
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  mo::standalone::resolveSettingsPath(g_settings);
  const bool firstRun = !mo::standalone::settingsFileExists(g_settings);
  mo::standalone::loadSettings(g_settings);
  g_spectrum.setResponse(g_appearance.spectrumResponse);
  updateComboName();
  updateCornerName();

  // --renderer <name> overrides the saved choice for this session.
  std::string requested = utf8(g_settings.sdlRenderer);
  for (int i = 1; i + 1 < argc; ++i)
    if (std::string_view(argv[i]) == "--renderer") requested = argv[i + 1];

  SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "0");
  if (!SDL_Init(SDL_INIT_VIDEO))
  {
    MessageBoxW(nullptr, widen(std::string("SDL could not be initialized: ") + SDL_GetError()).c_str(), L"Nihon's Music Framework", MB_ICONERROR);
    return 1;
  }

  g_rendererChoices.push_back("auto");
  for (int i = 0; i < SDL_GetNumRenderDrivers(); ++i)
    if (const char* name = SDL_GetRenderDriver(i)) g_rendererChoices.emplace_back(name);

  const int width = g_settings.windowedMode ? 640 : collapsedWidth();
  const int height = g_settings.windowedMode ? 240 : collapsedHeight();
  g_window = SDL_CreateWindow("Nihon's Music Framework - SDL", width, height, SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_HIDDEN);
  if (!g_window) { SDL_Quit(); return 2; }
  if (!createRenderer(requested)) { SDL_DestroyWindow(g_window); SDL_Quit(); return 3; }
  updateRendererLabel();

  // Some renderers recreate the native window, so query the handle afterwards.
  g_hwnd = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(g_window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
  if (!g_hwnd || !g_painter.initialize())
  {
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    SDL_Quit();
    return 4;
  }

  HINSTANCE instance = GetModuleHandleW(nullptr);
  if (HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(IDR_BANNER_PNG), RT_RCDATA))
    if (HGLOBAL data = LoadResource(instance, resource)) { g_bannerData = static_cast<const std::uint8_t*>(LockResource(data)); g_bannerSize = SizeofResource(instance, resource); }
  HICON icon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_MUSICOVERLAY), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
  HICON smallIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_MUSICOVERLAY), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
  if (icon) SendMessageW(g_hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
  if (smallIcon) SendMessageW(g_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));

  SetWindowPos(g_hwnd, nullptr, GetSystemMetrics(SM_CXSCREEN) - width - 28, 28, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
  mo::standalone::SettingsBindings bindings{&g_appearance, &g_settings.discord, &g_settings.backend, &g_settings.windowedMode, &g_settings.fontFamily, nullptr, settingsApply, settingsRecordShortcut, settingsRendererName, settingsCycleRenderer};
  g_settingsWindow.initialize(instance, icon, bindings);
  applyWindowMode();
  SDL_ShowWindow(g_window);
  if (!g_settings.windowedMode) ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
  buildTray(smallIcon ? smallIcon : icon);
  if (firstRun) saveSettings();
  g_media.start();

  while (g_running)
  {
    SDL_Event event{};
    if (SDL_WaitEventTimeout(&event, 8))
    {
      do
      {
        switch (event.type)
        {
          case SDL_EVENT_QUIT: g_running = false; break;
          case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT) handleMouseDown(event.button.x, event.button.y);
            break;
          case SDL_EVENT_WINDOW_MOVED:
            if (!g_settings.windowedMode) { if (g_appearance.overkill) positionOverkill(); else constrainToMonitor(); }
            break;
          default: break;
        }
      } while (SDL_PollEvent(&event));
    }
    tick();
    render();
  }

  g_discordRpc.disconnect();
  g_spectrum.stop();
  g_media.stop();
  saveSettings();
  if (g_tray) SDL_DestroyTray(g_tray);
  if (g_texture) SDL_DestroyTexture(g_texture);
  SDL_DestroyRenderer(g_renderer);
  SDL_DestroyWindow(g_window);
  SDL_Quit();
  if (icon) DestroyIcon(icon);
  if (smallIcon) DestroyIcon(smallIcon);
  return 0;
}
