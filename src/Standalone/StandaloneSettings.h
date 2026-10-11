#pragma once
#include <MusicOverlay/Renderer/IRenderer.h>
#include <string>
#include <vector>

#include "Platform/Windows/DiscordRpc.h"

namespace mo::standalone {
// Persistent configuration shared by the native, no-CRT, and SDL frontends.
// All frontends read and write the same %LOCALAPPDATA% settings.ini file.
struct StandaloneSettings {
  Appearance appearance;
  win::DiscordRpcSettings discord;
  int backend{11};
  bool windowedMode{};
  std::wstring fontFamily{L"Segoe UI Variable Display"};
  std::vector<int> shortcut{0x10}; // VK_SHIFT
  std::wstring sdlRenderer{L"opengl"}; // SDL render driver name, or "auto"
  std::wstring path;

  StandaloneSettings() = default;
  // appearance.fontFamily points into this object, so copies would dangle.
  StandaloneSettings(const StandaloneSettings&) = delete;
  StandaloneSettings& operator=(const StandaloneSettings&) = delete;
};

// Resolves %LOCALAPPDATA%\NihonsMusicFramework\settings.ini and creates the directory.
void resolveSettingsPath(StandaloneSettings& settings);
[[nodiscard]] bool settingsFileExists(const StandaloneSettings& settings);
void loadSettings(StandaloneSettings& settings);
void saveSettings(const StandaloneSettings& settings);
}
