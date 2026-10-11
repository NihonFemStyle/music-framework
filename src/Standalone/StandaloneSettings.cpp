#include "StandaloneSettings.h"

#include <algorithm>
#include <cstdlib>
#include <cwchar>
#include <windows.h>

namespace mo::standalone {
namespace {
const wchar_t* flag(bool value) { return value ? L"1" : L"0"; }
void writeString(const StandaloneSettings& s, const wchar_t* section, const wchar_t* key, const std::wstring& value) { WritePrivateProfileStringW(section, key, value.c_str(), s.path.c_str()); }
void writeNumber(const StandaloneSettings& s, const wchar_t* section, const wchar_t* key, long long value) { writeString(s, section, key, std::to_wstring(value)); }
void writeFlag(const StandaloneSettings& s, const wchar_t* key, bool value) { WritePrivateProfileStringW(L"Overlay", key, flag(value), s.path.c_str()); }
UINT readNumber(const StandaloneSettings& s, const wchar_t* section, const wchar_t* key, UINT fallback) { return GetPrivateProfileIntW(section, key, static_cast<INT>(fallback), s.path.c_str()); }
bool readFlag(const StandaloneSettings& s, const wchar_t* key, bool fallback) { return GetPrivateProfileIntW(L"Overlay", key, fallback ? 1 : 0, s.path.c_str()) != 0; }
std::wstring readString(const StandaloneSettings& s, const wchar_t* section, const wchar_t* key, const wchar_t* fallback)
{
  wchar_t value[256]{};
  GetPrivateProfileStringW(section, key, fallback, value, 256, s.path.c_str());
  return value;
}
std::uint32_t clampValue(UINT value, UINT minimum, UINT maximum) { return static_cast<std::uint32_t>(std::clamp(value, minimum, maximum)); }
}

void resolveSettingsPath(StandaloneSettings& s)
{
  wchar_t local[MAX_PATH]{};
  GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
  std::wstring directory = std::wstring(local) + L"\\NihonsMusicFramework";
  CreateDirectoryW(directory.c_str(), nullptr);
  s.path = directory + L"\\settings.ini";
}

bool settingsFileExists(const StandaloneSettings& s) { return GetFileAttributesW(s.path.c_str()) != INVALID_FILE_ATTRIBUTES; }

void loadSettings(StandaloneSettings& s)
{
  auto& a = s.appearance;
  a.opacity = std::clamp(static_cast<int>(readNumber(s, L"Overlay", L"Opacity", 92)) / 100.f, .1f, 1.f);
  a.accentRgb = readNumber(s, L"Overlay", L"Accent", 0x68D5C8);
  a.compact = readFlag(s, L"Compact", false);
  a.artworkBackground = readFlag(s, L"ArtworkBackground", false);
  a.dynamicAccent = readFlag(s, L"DynamicAccent", false);
  a.overkill = readFlag(s, L"Overkill", false);
  a.showBanner = readFlag(s, L"ShowBanner", true);
  a.ignoreParenthetical = readFlag(s, L"IgnoreParenthetical", false);
  a.corner = clampValue(readNumber(s, L"Overlay", L"Corner", 1), 0u, 3u);
  a.spectrumChroma = readFlag(s, L"SpectrumChroma", false);
  a.spectrumCurves = readFlag(s, L"SpectrumCurves", false);
  a.artworkSpectrum = readFlag(s, L"ArtworkSpectrum", false);
  a.spectrumBars = clampValue(readNumber(s, L"Overlay", L"SpectrumBars", 32), 8u, 64u);
  a.spectrumDelayMs = clampValue(readNumber(s, L"Overlay", L"SpectrumDelay", 0), 0u, 1000u);
  a.spectrumResponse = clampValue(readNumber(s, L"Overlay", L"SpectrumResponse", 1), 0u, 2u);
  a.spectrumMaxHeight = clampValue(readNumber(s, L"Overlay", L"SpectrumMaxHeight", 100), 10u, 100u);
  a.spectrumFade = clampValue(readNumber(s, L"Overlay", L"SpectrumFade", 10), 0u, 100u);
  a.spectrumHueSpeed = clampValue(readNumber(s, L"Overlay", L"SpectrumHueSpeed", 30), 0u, 360u);
  s.backend = static_cast<int>(readNumber(s, L"Overlay", L"Backend", 11));
  s.windowedMode = readFlag(s, L"WindowedMode", false);

  s.fontFamily = readString(s, L"Overlay", L"FontFamily", L"Segoe UI Variable Display");
  if (s.fontFamily.empty()) s.fontFamily = L"Segoe UI Variable Display";
  a.fontFamily = s.fontFamily.c_str();

  s.sdlRenderer = readString(s, L"SDL", L"Renderer", L"opengl");
  if (s.sdlRenderer.empty()) s.sdlRenderer = L"opengl";

  const std::wstring keys = readString(s, L"Overlay", L"Shortcut", L"16");
  s.shortcut.clear();
  const wchar_t* cursor = keys.c_str();
  while (*cursor)
  {
    wchar_t* end{};
    const long value = wcstol(cursor, &end, 10);
    if (end == cursor) break;
    if (value > 0 && value < 256) s.shortcut.push_back(static_cast<int>(value));
    cursor = (*end == L',') ? end + 1 : end;
  }
  if (s.shortcut.empty()) s.shortcut = {VK_SHIFT};

  s.discord.enabled = GetPrivateProfileIntW(L"DiscordRPC", L"Enabled", 0, s.path.c_str()) != 0;
  s.discord.applicationId = readString(s, L"DiscordRPC", L"ApplicationId", L"1061319148602392616");
  s.discord.defaultIconKey = readString(s, L"DiscordRPC", L"DefaultIconKey", L"logo");
  s.discord.artworkKey = readString(s, L"DiscordRPC", L"ArtworkKey", L"");
}

void saveSettings(const StandaloneSettings& s)
{
  const auto& a = s.appearance;
  writeNumber(s, L"Overlay", L"Opacity", static_cast<int>(a.opacity * 100));
  writeNumber(s, L"Overlay", L"Accent", a.accentRgb);
  writeFlag(s, L"Compact", a.compact);
  writeFlag(s, L"ArtworkBackground", a.artworkBackground);
  writeFlag(s, L"DynamicAccent", a.dynamicAccent);
  writeFlag(s, L"Overkill", a.overkill);
  writeNumber(s, L"Overlay", L"Corner", a.corner);
  writeFlag(s, L"ShowBanner", a.showBanner);
  writeFlag(s, L"IgnoreParenthetical", a.ignoreParenthetical);
  writeFlag(s, L"SpectrumChroma", a.spectrumChroma);
  writeFlag(s, L"SpectrumCurves", a.spectrumCurves);
  writeFlag(s, L"ArtworkSpectrum", a.artworkSpectrum);
  writeNumber(s, L"Overlay", L"SpectrumBars", a.spectrumBars);
  writeNumber(s, L"Overlay", L"SpectrumDelay", a.spectrumDelayMs);
  writeNumber(s, L"Overlay", L"SpectrumResponse", a.spectrumResponse);
  writeNumber(s, L"Overlay", L"SpectrumMaxHeight", a.spectrumMaxHeight);
  writeNumber(s, L"Overlay", L"SpectrumFade", a.spectrumFade);
  writeNumber(s, L"Overlay", L"SpectrumHueSpeed", a.spectrumHueSpeed);
  writeNumber(s, L"Overlay", L"Backend", s.backend);
  writeFlag(s, L"WindowedMode", s.windowedMode);
  writeString(s, L"Overlay", L"FontFamily", s.fontFamily);
  writeString(s, L"SDL", L"Renderer", s.sdlRenderer);
  std::wstring keys;
  for (std::size_t i = 0; i < s.shortcut.size(); ++i) { if (i) keys += L","; keys += std::to_wstring(s.shortcut[i]); }
  writeString(s, L"Overlay", L"Shortcut", keys);
  WritePrivateProfileStringW(L"DiscordRPC", L"Enabled", flag(s.discord.enabled), s.path.c_str());
  writeString(s, L"DiscordRPC", L"ApplicationId", s.discord.applicationId);
  writeString(s, L"DiscordRPC", L"DefaultIconKey", s.discord.defaultIconKey);
  writeString(s, L"DiscordRPC", L"ArtworkKey", s.discord.artworkKey);
}
}
