#pragma once
#include <MusicOverlay/Renderer/IRenderer.h>
#include "Platform/Windows/DiscordRpc.h"
#include <memory>
#include <string>
#include <windows.h>
namespace mo::standalone {
struct SettingsBindings { Appearance* appearance{}; win::DiscordRpcSettings* discord{}; int* backend{}; bool* windowedMode{}; std::wstring* fontFamily{}; void* context{}; void (*apply)(void*){}; void (*recordShortcut)(void*){};
  // Optional renderer overrides used by frontends whose renderer list is not the DirectX backend list (SDL).
  const wchar_t* (*rendererName)(void*){}; void (*cycleRenderer)(void*){}; };

// Native Direct2D/DirectWrite configuration window styled after the
// CyberiaStyle LAST APPROACH options screens. Every visual element is drawn
// procedurally; no theme assets are embedded.
class SettingsWindow {
public:
  SettingsWindow();~SettingsWindow();
  bool initialize(HINSTANCE,HICON,SettingsBindings);void show(HWND);void refresh();void refreshShortcut();void chooseFont();[[nodiscard]] HWND hwnd()const{return hwnd_;}
private:
  struct Impl;static LRESULT CALLBACK windowProc(HWND,UINT,WPARAM,LPARAM);LRESULT handleMessage(UINT,WPARAM,LPARAM);
  void paint();void click(float,float);void hover(float,float);void drag(float);void chooseAccent();void apply();void fitToDpi();
  HINSTANCE instance_{};HICON icon_{};HWND hwnd_{};SettingsBindings bindings_{};std::unique_ptr<Impl> impl_;
};
}
