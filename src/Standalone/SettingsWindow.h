#pragma once
#include <MusicOverlay/Renderer/IRenderer.h>
#include "Platform/Windows/DiscordRpc.h"
#include <memory>
#include <string>
#include <windows.h>
namespace mo::standalone {
struct SettingsBindings { Appearance* appearance{}; win::DiscordRpcSettings* discord{}; int* backend{}; bool* windowedMode{}; std::wstring* fontFamily{}; void* context{}; void (*apply)(void*){}; void (*recordShortcut)(void*){}; };
class SettingsWindow {
public:
  SettingsWindow();~SettingsWindow();
  bool initialize(HINSTANCE,HICON,SettingsBindings);void show(HWND);void refresh();void refreshShortcut();void chooseFont();[[nodiscard]] HWND hwnd()const{return hwnd_;}
private:
  struct Impl;static LRESULT CALLBACK windowProc(HWND,UINT,WPARAM,LPARAM);LRESULT handleMessage(UINT,WPARAM,LPARAM);void paint();void click(float,float);void chooseAccent();void apply();
  HINSTANCE instance_{};HICON icon_{};HWND hwnd_{};SettingsBindings bindings_{};std::unique_ptr<Impl> impl_;
};
}
