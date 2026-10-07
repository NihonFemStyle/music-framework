#include <windows.h>
#include <windowsx.h>

#include <MusicOverlay/Integration/MusicOverlay.h>
#include <algorithm>
#include <array>
#include <commctrl.h>
#include <commdlg.h>
#include <cstdint>
#include <d3d11.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <memory>
#include <shellapi.h>
#include <string>
#include <vector>
#include <winrt/base.h>
#include <wrl/client.h>

#include "Platform/Windows/MediaSessionManager.h"
#include "Platform/Windows/ProcessSpectrumCapture.h"
#include "Platform/Windows/DiscordRpc.h"
#include "SettingsWindow.h"
#include "resource.h"
using Microsoft::WRL::ComPtr;
namespace
{
constexpr wchar_t kClassName[]=L"NihonsMusicFrameworkWindow";
constexpr UINT_PTR kTimer=1;
constexpr UINT kTrayMessage=WM_APP+1;
constexpr UINT kTrayId=1;
constexpr UINT kMenuSettings=1001,kMenuToggle=1002,kMenuExit=1003,kMenuCompact=1010,kMenuDynamicAccent=1011,kMenuArtworkBackground=1012,kMenuOverkill=1013,kMenuBanner=1014,kMenuAccent=1015,kMenuShortcut=1016,kMenuIgnoreParenthetical=1017,kMenuSpectrumChroma=1018,kMenuSpectrumCurves=1019,kMenuArtworkSpectrum=1020,kMenuDiscordRpc=1021,kMenuWindowed=1022,kMenuFont=1023,kMenuOpacityBase=1100,kMenuCornerBase=1200,kMenuBackendBase=1300,kMenuBarCountBase=1400,kMenuDelayBase=1410,kMenuResponseBase=1420,kMenuHeightBase=1430,kMenuFadeBase=1440,kMenuHueBase=1450;
constexpr std::array<UINT,5> kBarCounts{16,24,32,48,64};
constexpr std::array<UINT,5> kSpectrumDelays{0,50,100,200,350};
constexpr std::array<UINT,3> kSpectrumResponses{0,1,2};
constexpr std::array<UINT,4> kSpectrumHeights{25,50,75,100};
constexpr std::array<UINT,5> kSpectrumFades{0,5,10,20,30};
constexpr std::array<UINT,4> kHueSpeeds{15,30,60,120};
constexpr int kNormalWidth=430,kNormalHeight=112,kCompactWidth=330,kCompactHeight=76,kOverkillWidth=520,kOverkillHeight=180,kSettingsHeight=534;
mo::OverlayState g_state; mo::win::MediaSessionManager g_media(g_state);
std::unique_ptr<mo::IntegrationContext> g_overlay; ComPtr<ID3D11Device> g_device; ComPtr<ID3D11DeviceContext> g_context; ComPtr<IDXGISwapChain> g_swap;ComPtr<IDCompositionDevice> g_composition;ComPtr<IDCompositionTarget> g_compositionTarget;ComPtr<IDCompositionVisual> g_compositionVisual;ComPtr<IDCompositionEffectGroup> g_compositionEffects;
mo::Appearance g_appearance; const std::uint8_t* g_bannerData{}; std::uint32_t g_bannerSize{};
std::wstring g_settingsPath,g_comboName=L"Shift",g_fontFamily=L"Segoe UI Variable Display"; std::vector<int> g_combo{VK_SHIFT},g_recorded;
bool g_interactive{},g_comboWasDown{},g_recording{}; ULONGLONG g_lastTap{},g_lastMediaPoll{}; bool g_scanWasDown[256]{}; int g_backend=11;
constexpr ULONGLONG kInteractionDoubleTapMs=450;
NOTIFYICONDATAW g_tray{};
int g_targetHeight{};bool g_settingsClosing{};
mo::win::ProcessSpectrumCapture g_spectrum;ULONGLONG g_lastSpectrumCheck{};
mo::win::DiscordRpc g_discordRpc;mo::win::DiscordRpcSettings g_discordSettings;
mo::standalone::SettingsWindow g_settingsWindow;bool g_windowedMode{};

std::wstring keyName(int vk){wchar_t name[64]{};UINT scan=MapVirtualKeyW(vk,MAPVK_VK_TO_VSC)<<16;if(vk==VK_LEFT||vk==VK_UP||vk==VK_RIGHT||vk==VK_DOWN||vk==VK_RCONTROL||vk==VK_RMENU)scan|=1<<24;if(GetKeyNameTextW(static_cast<LONG>(scan),name,64)>0)return name;wchar_t fallback[16]{};wsprintfW(fallback,L"VK %02X",vk);return fallback;}
void updateComboName(){g_comboName.clear();for(size_t i=0;i<g_combo.size();++i){if(i)g_comboName+=L" + ";g_comboName+=keyName(g_combo[i]);}g_appearance.activationKeyName=g_recording?L"Press keys...":g_comboName.c_str();}
bool comboDown(){for(int vk:g_combo)if(!(GetAsyncKeyState(vk)&0x8000))return false;return !g_combo.empty();}
void saveSettings(){WritePrivateProfileStringW(L"Overlay",L"Opacity",std::to_wstring(static_cast<int>(g_appearance.opacity*100)).c_str(),g_settingsPath.c_str());WritePrivateProfileStringW(L"Overlay",L"Accent",std::to_wstring(g_appearance.accentRgb).c_str(),g_settingsPath.c_str());WritePrivateProfileStringW(L"Overlay",L"Compact",g_appearance.compact?L"1":L"0",g_settingsPath.c_str());WritePrivateProfileStringW(L"Overlay",L"ArtworkBackground",g_appearance.artworkBackground?L"1":L"0",g_settingsPath.c_str());WritePrivateProfileStringW(L"Overlay",L"DynamicAccent",g_appearance.dynamicAccent?L"1":L"0",g_settingsPath.c_str());WritePrivateProfileStringW(L"Overlay",L"Overkill",g_appearance.overkill?L"1":L"0",g_settingsPath.c_str());WritePrivateProfileStringW(L"Overlay",L"Corner",std::to_wstring(g_appearance.corner).c_str(),g_settingsPath.c_str());WritePrivateProfileStringW(L"Overlay",L"ShowBanner",g_appearance.showBanner?L"1":L"0",g_settingsPath.c_str());WritePrivateProfileStringW(L"Overlay",L"IgnoreParenthetical",g_appearance.ignoreParenthetical?L"1":L"0",g_settingsPath.c_str());WritePrivateProfileStringW(L"Overlay",L"SpectrumChroma",g_appearance.spectrumChroma?L"1":L"0",g_settingsPath.c_str());WritePrivateProfileStringW(L"Overlay",L"SpectrumCurves",g_appearance.spectrumCurves?L"1":L"0",g_settingsPath.c_str());WritePrivateProfileStringW(L"Overlay",L"ArtworkSpectrum",g_appearance.artworkSpectrum?L"1":L"0",g_settingsPath.c_str());WritePrivateProfileStringW(L"Overlay",L"SpectrumBars",std::to_wstring(g_appearance.spectrumBars).c_str(),g_settingsPath.c_str());WritePrivateProfileStringW(L"Overlay",L"SpectrumDelay",std::to_wstring(g_appearance.spectrumDelayMs).c_str(),g_settingsPath.c_str());WritePrivateProfileStringW(L"Overlay",L"SpectrumResponse",std::to_wstring(g_appearance.spectrumResponse).c_str(),g_settingsPath.c_str());WritePrivateProfileStringW(L"Overlay",L"SpectrumMaxHeight",std::to_wstring(g_appearance.spectrumMaxHeight).c_str(),g_settingsPath.c_str());WritePrivateProfileStringW(L"Overlay",L"SpectrumFade",std::to_wstring(g_appearance.spectrumFade).c_str(),g_settingsPath.c_str());WritePrivateProfileStringW(L"Overlay",L"SpectrumHueSpeed",std::to_wstring(g_appearance.spectrumHueSpeed).c_str(),g_settingsPath.c_str());WritePrivateProfileStringW(L"Overlay",L"Backend",std::to_wstring(g_backend).c_str(),g_settingsPath.c_str());WritePrivateProfileStringW(L"DiscordRPC",L"Enabled",g_discordSettings.enabled?L"1":L"0",g_settingsPath.c_str());WritePrivateProfileStringW(L"DiscordRPC",L"ApplicationId",g_discordSettings.applicationId.c_str(),g_settingsPath.c_str());WritePrivateProfileStringW(L"DiscordRPC",L"DefaultIconKey",g_discordSettings.defaultIconKey.c_str(),g_settingsPath.c_str());WritePrivateProfileStringW(L"DiscordRPC",L"ArtworkKey",g_discordSettings.artworkKey.c_str(),g_settingsPath.c_str());std::wstring keys;for(size_t i=0;i<g_combo.size();++i){if(i)keys+=L",";keys+=std::to_wstring(g_combo[i]);}WritePrivateProfileStringW(L"Overlay",L"Shortcut",keys.c_str(),g_settingsPath.c_str());}
void loadSettings(){wchar_t local[MAX_PATH]{};GetEnvironmentVariableW(L"LOCALAPPDATA",local,MAX_PATH);std::wstring dir=std::wstring(local)+L"\\NihonsMusicFramework";CreateDirectoryW(dir.c_str(),nullptr);g_settingsPath=dir+L"\\settings.ini";if(GetFileAttributesW(g_settingsPath.c_str())==INVALID_FILE_ATTRIBUTES){TASKDIALOG_BUTTON buttons[]={{9,L"DirectX 9"},{10,L"DirectX 10"},{11,L"DirectX 11 (recommended)"},{12,L"DirectX 12"}};TASKDIALOGCONFIG c{};c.cbSize=sizeof(c);c.dwFlags=TDF_USE_COMMAND_LINKS;c.pszWindowTitle=L"Nihon's Music Framework";c.pszMainInstruction=L"Choose a renderer";c.pszContent=L"You can change this later. A renderer change takes effect after restart.";c.pButtons=buttons;c.cButtons=4;c.nDefaultButton=11;int selected=11;TaskDialogIndirect(&c,&selected,nullptr,nullptr);g_backend=selected;saveSettings();}g_appearance.opacity=std::clamp(GetPrivateProfileIntW(L"Overlay",L"Opacity",92,g_settingsPath.c_str())/100.f,.1f,1.f);g_appearance.accentRgb=GetPrivateProfileIntW(L"Overlay",L"Accent",0x68D5C8,g_settingsPath.c_str());g_appearance.compact=GetPrivateProfileIntW(L"Overlay",L"Compact",0,g_settingsPath.c_str())!=0;g_appearance.artworkBackground=GetPrivateProfileIntW(L"Overlay",L"ArtworkBackground",0,g_settingsPath.c_str())!=0;g_appearance.dynamicAccent=GetPrivateProfileIntW(L"Overlay",L"DynamicAccent",0,g_settingsPath.c_str())!=0;g_appearance.overkill=GetPrivateProfileIntW(L"Overlay",L"Overkill",0,g_settingsPath.c_str())!=0;g_appearance.showBanner=GetPrivateProfileIntW(L"Overlay",L"ShowBanner",1,g_settingsPath.c_str())!=0;g_appearance.ignoreParenthetical=GetPrivateProfileIntW(L"Overlay",L"IgnoreParenthetical",0,g_settingsPath.c_str())!=0;g_appearance.corner=static_cast<std::uint32_t>(std::clamp<int>(static_cast<int>(GetPrivateProfileIntW(L"Overlay",L"Corner",1,g_settingsPath.c_str())),0,3));g_backend=GetPrivateProfileIntW(L"Overlay",L"Backend",11,g_settingsPath.c_str());wchar_t keys[256]{};GetPrivateProfileStringW(L"Overlay",L"Shortcut",L"16",keys,256,g_settingsPath.c_str());g_combo.clear();wchar_t* p=keys;while(*p){wchar_t* end{};long value=wcstol(p,&end,10);if(end==p)break;if(value>0&&value<256)g_combo.push_back(static_cast<int>(value));p=(*end==L',')?end+1:end;}if(g_combo.empty())g_combo={VK_SHIFT};updateComboName();}
void loadSpectrumSettings(){g_appearance.spectrumChroma=GetPrivateProfileIntW(L"Overlay",L"SpectrumChroma",0,g_settingsPath.c_str())!=0;g_appearance.spectrumCurves=GetPrivateProfileIntW(L"Overlay",L"SpectrumCurves",0,g_settingsPath.c_str())!=0;g_appearance.artworkSpectrum=GetPrivateProfileIntW(L"Overlay",L"ArtworkSpectrum",0,g_settingsPath.c_str())!=0;g_appearance.spectrumBars=static_cast<std::uint32_t>(std::clamp<UINT>(GetPrivateProfileIntW(L"Overlay",L"SpectrumBars",32,g_settingsPath.c_str()),8u,64u));g_appearance.spectrumDelayMs=static_cast<std::uint32_t>(std::clamp<UINT>(GetPrivateProfileIntW(L"Overlay",L"SpectrumDelay",0,g_settingsPath.c_str()),0u,1000u));g_appearance.spectrumResponse=static_cast<std::uint32_t>(std::clamp<UINT>(GetPrivateProfileIntW(L"Overlay",L"SpectrumResponse",1,g_settingsPath.c_str()),0u,2u));g_appearance.spectrumMaxHeight=static_cast<std::uint32_t>(std::clamp<UINT>(GetPrivateProfileIntW(L"Overlay",L"SpectrumMaxHeight",100,g_settingsPath.c_str()),10u,100u));g_appearance.spectrumFade=static_cast<std::uint32_t>(std::clamp<UINT>(GetPrivateProfileIntW(L"Overlay",L"SpectrumFade",10,g_settingsPath.c_str()),0u,100u));g_appearance.spectrumHueSpeed=static_cast<std::uint32_t>(std::clamp<UINT>(GetPrivateProfileIntW(L"Overlay",L"SpectrumHueSpeed",30,g_settingsPath.c_str()),0u,360u));g_spectrum.setResponse(g_appearance.spectrumResponse);}
void loadDiscordSettings(){g_discordSettings.enabled=GetPrivateProfileIntW(L"DiscordRPC",L"Enabled",0,g_settingsPath.c_str())!=0;wchar_t value[256]{};GetPrivateProfileStringW(L"DiscordRPC",L"ApplicationId",L"1061319148602392616",value,256,g_settingsPath.c_str());g_discordSettings.applicationId=value;GetPrivateProfileStringW(L"DiscordRPC",L"DefaultIconKey",L"logo",value,256,g_settingsPath.c_str());g_discordSettings.defaultIconKey=value;GetPrivateProfileStringW(L"DiscordRPC",L"ArtworkKey",L"",value,256,g_settingsPath.c_str());g_discordSettings.artworkKey=value;}
void loadFontSetting(){wchar_t value[LF_FACESIZE]{};GetPrivateProfileStringW(L"Overlay",L"FontFamily",L"Segoe UI Variable Display",value,LF_FACESIZE,g_settingsPath.c_str());g_fontFamily=*value?value:L"Segoe UI Variable Display";g_appearance.fontFamily=g_fontFamily.c_str();}
void loadWindowedSetting(){g_windowedMode=GetPrivateProfileIntW(L"Overlay",L"WindowedMode",0,g_settingsPath.c_str())!=0;}
void saveWindowedSetting(){WritePrivateProfileStringW(L"Overlay",L"WindowedMode",g_windowedMode?L"1":L"0",g_settingsPath.c_str());}
void updateBackendName(){static const wchar_t* names[]{L"DirectX 9",L"DirectX 10",L"DirectX 11",L"DirectX 12"};int index=g_backend==9?0:g_backend==10?1:g_backend==12?3:2;g_appearance.backendName=names[index];}
void updateCornerName(){static const wchar_t* names[]{L"Top left",L"Top right",L"Bottom left",L"Bottom right"};g_appearance.cornerName=names[std::min<std::uint32_t>(g_appearance.corner,3)];}
bool createDevice(HWND hwnd)
{
  D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1,D3D_FEATURE_LEVEL_10_0};
  if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,3,D3D11_SDK_VERSION,&g_device,nullptr,&g_context)))return false;
  ComPtr<IDXGIDevice> dxgiDevice;if(FAILED(g_device.As(&dxgiDevice)))return false;ComPtr<IDXGIAdapter> adapter;if(FAILED(dxgiDevice->GetAdapter(&adapter)))return false;ComPtr<IDXGIFactory2> factory;if(FAILED(adapter->GetParent(IID_PPV_ARGS(&factory))))return false;
  RECT client{};GetClientRect(hwnd,&client);DXGI_SWAP_CHAIN_DESC1 desc{};desc.Width=std::max<LONG>(1,client.right);desc.Height=std::max<LONG>(1,client.bottom);desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.Scaling=DXGI_SCALING_STRETCH;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;desc.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;
  ComPtr<IDXGISwapChain1> compositionSwap;if(FAILED(factory->CreateSwapChainForComposition(g_device.Get(),&desc,nullptr,&compositionSwap)))return false;if(FAILED(compositionSwap.As(&g_swap)))return false;
  if(FAILED(DCompositionCreateDevice(dxgiDevice.Get(),IID_PPV_ARGS(&g_composition)))||FAILED(g_composition->CreateTargetForHwnd(hwnd,TRUE,&g_compositionTarget))||FAILED(g_composition->CreateVisual(&g_compositionVisual))||FAILED(g_composition->CreateEffectGroup(&g_compositionEffects))||FAILED(g_compositionVisual->SetEffect(g_compositionEffects.Get()))||FAILED(g_compositionVisual->SetContent(g_swap.Get()))||FAILED(g_compositionTarget->SetRoot(g_compositionVisual.Get()))||FAILED(g_composition->Commit()))return false;
  g_overlay=std::make_unique<mo::IntegrationContext>(mo::Backend::D3D11);return g_overlay->initialize({hwnd,g_device.Get(),g_swap.Get(),nullptr});
}
void applyInteraction(HWND hwnd){auto style=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);const bool interactive=g_windowedMode||g_interactive;auto wanted=interactive?(style&~WS_EX_TRANSPARENT):(style|WS_EX_TRANSPARENT);if(wanted!=style)SetWindowLongPtrW(hwnd,GWL_EXSTYLE,wanted);g_appearance.interactive=interactive;}
void applyOpacity(HWND hwnd)
{
  (void)hwnd;if(g_compositionEffects&&g_composition){g_compositionEffects->SetOpacity(std::clamp(g_appearance.opacity,.1f,1.f));g_composition->Commit();}
  }
  int collapsedHeight(){return g_appearance.overkill?kOverkillHeight:(g_appearance.compact?kCompactHeight:kNormalHeight);}
  int collapsedWidth(){return g_appearance.overkill?kOverkillWidth:(g_appearance.compact?kCompactWidth:kNormalWidth);}
  void constrainToMonitor(HWND hwnd){RECT window{};if(!GetWindowRect(hwnd,&window))return;HMONITOR monitor=MonitorFromRect(&window,MONITOR_DEFAULTTONEAREST);MONITORINFO info{sizeof(info)};if(!GetMonitorInfoW(monitor,&info))return;LONG width=window.right-window.left,height=window.bottom-window.top;LONG x=window.left,y=window.top;if(width>=info.rcWork.right-info.rcWork.left)x=info.rcWork.left;else x=std::clamp(x,info.rcWork.left,info.rcWork.right-width);if(height>=info.rcWork.bottom-info.rcWork.top)y=info.rcWork.top;else y=std::clamp(y,info.rcWork.top,info.rcWork.bottom-height);if(x!=window.left||y!=window.top)SetWindowPos(hwnd,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);}
  void positionOverkill(HWND hwnd){if(g_windowedMode||!g_appearance.overkill||g_appearance.settingsOpen)return;HMONITOR monitor=MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST);MONITORINFO info{sizeof(info)};GetMonitorInfoW(monitor,&info);RECT bounds=info.rcWork;HWND foreground=GetForegroundWindow();if(foreground&&foreground!=hwnd){RECT window{};GetWindowRect(foreground,&window);HMONITOR foregroundMonitor=MonitorFromWindow(foreground,MONITOR_DEFAULTTONEAREST);MONITORINFO foregroundInfo{sizeof(foregroundInfo)};GetMonitorInfoW(foregroundMonitor,&foregroundInfo);if(abs(window.left-foregroundInfo.rcMonitor.left)<=2&&abs(window.top-foregroundInfo.rcMonitor.top)<=2&&abs(window.right-foregroundInfo.rcMonitor.right)<=2&&abs(window.bottom-foregroundInfo.rcMonitor.bottom)<=2){monitor=foregroundMonitor;bounds=foregroundInfo.rcMonitor;}}int x=(g_appearance.corner==0||g_appearance.corner==2)?bounds.left:bounds.right-kOverkillWidth;int y=(g_appearance.corner<2)?bounds.top:bounds.bottom-kOverkillHeight;SetWindowPos(hwnd,HWND_TOPMOST,x,y,kOverkillWidth,kOverkillHeight,SWP_NOACTIVATE);}
  void resizeWindow(HWND hwnd){if(g_windowedMode){InvalidateRect(hwnd,nullptr,FALSE);return;}RECT r{};GetWindowRect(hwnd,&r);int w=collapsedWidth();int h=collapsedHeight();SetWindowPos(hwnd,HWND_TOPMOST,r.left,r.top,w,h,SWP_NOACTIVATE);if(g_appearance.overkill)positionOverkill(hwnd);else constrainToMonitor(hwnd);}
  void transitionSettings(HWND hwnd,bool open){RECT r{};GetWindowRect(hwnd,&r);g_settingsClosing=!open;if(open)g_appearance.settingsOpen=true;g_targetHeight=open?kSettingsHeight:collapsedHeight();int width=open?kNormalWidth:collapsedWidth();SetWindowPos(hwnd,HWND_TOPMOST,r.left,r.top,width,r.bottom-r.top,SWP_NOACTIVATE);constrainToMonitor(hwnd);InvalidateRect(hwnd,nullptr,FALSE);}
  void animateSettings(HWND hwnd){if(!g_targetHeight)return;RECT r{};GetWindowRect(hwnd,&r);int current=r.bottom-r.top,difference=g_targetHeight-current;if(abs(difference)<=2){SetWindowPos(hwnd,HWND_TOPMOST,r.left,r.top,g_settingsClosing?collapsedWidth():r.right-r.left,g_targetHeight,SWP_NOACTIVATE);constrainToMonitor(hwnd);g_targetHeight=0;if(g_settingsClosing){g_appearance.settingsOpen=false;g_settingsClosing=false;if(g_appearance.overkill)positionOverkill(hwnd);InvalidateRect(hwnd,nullptr,FALSE);}return;}int magnitude=std::min(abs(difference),std::clamp(abs(difference)/4,4,28));int step=difference<0?-magnitude:magnitude;SetWindowPos(hwnd,HWND_TOPMOST,r.left,r.top,r.right-r.left,current+step,SWP_NOACTIVATE);constrainToMonitor(hwnd);}
  void applyWindowMode(HWND hwnd)
  {
    RECT r{};GetWindowRect(hwnd,&r);
    if(g_windowedMode)
    {
      g_interactive=true;
      DWM_BLURBEHIND blur{DWM_BB_ENABLE,FALSE,nullptr,FALSE};DwmEnableBlurBehindWindow(hwnd,&blur);MARGINS margins{};DwmExtendFrameIntoClientArea(hwnd,&margins);
      SetWindowLongPtrW(hwnd,GWL_STYLE,WS_OVERLAPPEDWINDOW|WS_VISIBLE);
      SetWindowLongPtrW(hwnd,GWL_EXSTYLE,WS_EX_APPWINDOW|WS_EX_NOREDIRECTIONBITMAP);
      int width=std::max<LONG>(640,r.right-r.left),height=std::max<LONG>(240,r.bottom-r.top);
      SetWindowPos(hwnd,HWND_NOTOPMOST,r.left,r.top,width,height,SWP_FRAMECHANGED|SWP_SHOWWINDOW);
    }else
    {
      DWM_BLURBEHIND blur{DWM_BB_ENABLE,TRUE,nullptr,TRUE};DwmEnableBlurBehindWindow(hwnd,&blur);MARGINS margins{-1};DwmExtendFrameIntoClientArea(hwnd,&margins);
      SetWindowLongPtrW(hwnd,GWL_STYLE,WS_POPUP|WS_VISIBLE);
      LONG_PTR ex=WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOREDIRECTIONBITMAP|WS_EX_NOACTIVATE|(g_interactive?0:WS_EX_TRANSPARENT);
      SetWindowLongPtrW(hwnd,GWL_EXSTYLE,ex);
      SetWindowPos(hwnd,HWND_TOPMOST,r.left,r.top,collapsedWidth(),collapsedHeight(),SWP_FRAMECHANGED|SWP_NOACTIVATE|SWP_SHOWWINDOW);
      if(g_appearance.overkill)positionOverkill(hwnd);else constrainToMonitor(hwnd);
    }
    applyInteraction(hwnd);applyOpacity(hwnd);saveWindowedSetting();RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_FRAME|RDW_ALLCHILDREN);
  }
  void settingsApply(void* context){HWND hwnd=static_cast<HWND>(context);updateBackendName();updateCornerName();g_spectrum.setResponse(g_appearance.spectrumResponse);if(!g_discordSettings.enabled)g_discordRpc.disconnect();applyWindowMode(hwnd);resizeWindow(hwnd);saveSettings();WritePrivateProfileStringW(L"Overlay",L"FontFamily",g_fontFamily.c_str(),g_settingsPath.c_str());InvalidateRect(hwnd,nullptr,FALSE);}
  void settingsRecordShortcut(void*){g_recording=true;g_recorded.clear();ZeroMemory(g_scanWasDown,sizeof(g_scanWasDown));updateComboName();g_settingsWindow.refreshShortcut();}
  void openSettings(HWND hwnd){g_settingsWindow.show(hwnd);}
  void addTrayIcon(HWND hwnd,HICON icon){g_tray.cbSize=sizeof(g_tray);g_tray.hWnd=hwnd;g_tray.uID=kTrayId;g_tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP|NIF_SHOWTIP;g_tray.uCallbackMessage=kTrayMessage;g_tray.hIcon=icon;lstrcpynW(g_tray.szTip,L"Nihon's Music Framework",_countof(g_tray.szTip));Shell_NotifyIconW(NIM_ADD,&g_tray);g_tray.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&g_tray);}
  void showTrayMenu(HWND hwnd)
  {
    HMENU menu=CreatePopupMenu(),opacity=CreatePopupMenu(),corner=CreatePopupMenu(),backend=CreatePopupMenu(),bars=CreatePopupMenu(),delay=CreatePopupMenu(),response=CreatePopupMenu(),height=CreatePopupMenu(),fade=CreatePopupMenu(),hue=CreatePopupMenu();
    SYSTEMTIME localTime{};GetLocalTime(&localTime);const bool aprilFools=localTime.wMonth==4&&localTime.wDay==1;const wchar_t* hueRateSuffix=aprilFools?L"\u00C2\u00B0/s":L"\u00B0/s";
    for(int value:{10,25,50,75,100})AppendMenuW(opacity,MF_STRING|(static_cast<int>(g_appearance.opacity*100+.5f)==value?MF_CHECKED:0),kMenuOpacityBase+value,(std::to_wstring(value)+L"%").c_str());
    const wchar_t* corners[]{L"Top left",L"Top right",L"Bottom left",L"Bottom right"};for(UINT i=0;i<4;++i)AppendMenuW(corner,MF_STRING|(g_appearance.corner==i?MF_CHECKED:0),kMenuCornerBase+i,corners[i]);
    for(UINT value:{9u,10u,11u,12u})AppendMenuW(backend,MF_STRING|(g_backend==static_cast<int>(value)?MF_CHECKED:0),kMenuBackendBase+value,(L"DirectX "+std::to_wstring(value)).c_str());
    for(UINT i=0;i<kBarCounts.size();++i)AppendMenuW(bars,MF_STRING|(g_appearance.spectrumBars==kBarCounts[i]?MF_CHECKED:0),kMenuBarCountBase+i,std::to_wstring(kBarCounts[i]).c_str());
    for(UINT i=0;i<kSpectrumDelays.size();++i)AppendMenuW(delay,MF_STRING|(g_appearance.spectrumDelayMs==kSpectrumDelays[i]?MF_CHECKED:0),kMenuDelayBase+i,(std::to_wstring(kSpectrumDelays[i])+L" ms").c_str());
    const wchar_t* responseNames[]{L"Fast",L"Balanced",L"Smooth"};for(UINT i=0;i<kSpectrumResponses.size();++i)AppendMenuW(response,MF_STRING|(g_appearance.spectrumResponse==i?MF_CHECKED:0),kMenuResponseBase+i,responseNames[i]);
    for(UINT i=0;i<kSpectrumHeights.size();++i)AppendMenuW(height,MF_STRING|(g_appearance.spectrumMaxHeight==kSpectrumHeights[i]?MF_CHECKED:0),kMenuHeightBase+i,(std::to_wstring(kSpectrumHeights[i])+L"%").c_str());
    for(UINT i=0;i<kSpectrumFades.size();++i)AppendMenuW(fade,MF_STRING|(g_appearance.spectrumFade==kSpectrumFades[i]?MF_CHECKED:0),kMenuFadeBase+i,(std::to_wstring(kSpectrumFades[i])+L"%").c_str());
    for(UINT i=0;i<kHueSpeeds.size();++i)AppendMenuW(hue,MF_STRING|(g_appearance.spectrumHueSpeed==kHueSpeeds[i]?MF_CHECKED:0),kMenuHueBase+i,(std::to_wstring(kHueSpeeds[i])+hueRateSuffix).c_str());
    AppendMenuW(menu,MF_STRING,kMenuSettings,L"Open settings");AppendMenuW(menu,MF_STRING|(g_windowedMode?MF_CHECKED:0),kMenuWindowed,L"Windowed mode");AppendMenuW(menu,MF_STRING|(g_windowedMode?MF_GRAYED:0),kMenuToggle,g_interactive?L"Disable interaction":L"Enable interaction");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(opacity),L"Transparency");AppendMenuW(menu,MF_STRING|(g_appearance.compact?MF_CHECKED:0),kMenuCompact,L"Compact mode");AppendMenuW(menu,MF_STRING,kMenuAccent,L"Choose accent...");AppendMenuW(menu,MF_STRING,kMenuFont,L"Choose text font...");AppendMenuW(menu,MF_STRING|(g_appearance.dynamicAccent?MF_CHECKED:0),kMenuDynamicAccent,L"Dynamic accent");AppendMenuW(menu,MF_STRING|(g_appearance.artworkBackground?MF_CHECKED:0),kMenuArtworkBackground,L"Artwork background");AppendMenuW(menu,MF_STRING,kMenuShortcut,L"Set interaction shortcut...");AppendMenuW(menu,MF_STRING|(g_appearance.ignoreParenthetical?MF_CHECKED:0),kMenuIgnoreParenthetical,L"Ignore title parentheses");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(backend),L"Renderer (restart required)");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING|(g_appearance.overkill?MF_CHECKED:0),kMenuOverkill,L"Overkill mode");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(corner),L"Overkill corner");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(bars),L"Spectrum bars / points");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(delay),L"Spectrum delay");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(response),L"Spectrum response");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(height),L"Spectrum max height");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(fade),L"Spectrum top fade");AppendMenuW(menu,MF_STRING|(g_appearance.spectrumCurves?MF_CHECKED:0),kMenuSpectrumCurves,L"Curves style");AppendMenuW(menu,MF_STRING|(g_appearance.spectrumChroma?MF_CHECKED:0),kMenuSpectrumChroma,L"Chroma spectrum");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(hue),L"Chroma hue shift");AppendMenuW(menu,MF_STRING|(g_appearance.artworkSpectrum?MF_CHECKED:0),kMenuArtworkSpectrum,L"Artwork spectrum");AppendMenuW(menu,MF_STRING|(g_appearance.showBanner?MF_CHECKED:0),kMenuBanner,L"Show banner");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING|(g_discordSettings.enabled?MF_CHECKED:0),kMenuDiscordRpc,L"Discord Rich Presence");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,kMenuExit,L"Exit");POINT point{};GetCursorPos(&point);SetForegroundWindow(hwnd);TrackPopupMenu(menu,TPM_RIGHTBUTTON|TPM_BOTTOMALIGN|TPM_LEFTALIGN,point.x,point.y,0,hwnd,nullptr);DestroyMenu(menu);PostMessageW(hwnd,WM_NULL,0,0);
  }
  bool handleTraySetting(HWND hwnd,UINT command)
  {
    if(command>kMenuOpacityBase&&command<=kMenuOpacityBase+100){g_appearance.opacity=float(command-kMenuOpacityBase)/100.f;applyOpacity(hwnd);}
    else if(command>=kMenuCornerBase&&command<kMenuCornerBase+4){g_appearance.corner=command-kMenuCornerBase;updateCornerName();if(g_appearance.overkill&&!g_appearance.settingsOpen)positionOverkill(hwnd);}
    else if(command>=kMenuBackendBase+9&&command<=kMenuBackendBase+12){g_backend=static_cast<int>(command-kMenuBackendBase);updateBackendName();}
    else if(command>=kMenuBarCountBase&&command<kMenuBarCountBase+kBarCounts.size())g_appearance.spectrumBars=kBarCounts[command-kMenuBarCountBase];
    else if(command>=kMenuDelayBase&&command<kMenuDelayBase+kSpectrumDelays.size())g_appearance.spectrumDelayMs=kSpectrumDelays[command-kMenuDelayBase];
    else if(command>=kMenuResponseBase&&command<kMenuResponseBase+kSpectrumResponses.size()){g_appearance.spectrumResponse=kSpectrumResponses[command-kMenuResponseBase];g_spectrum.setResponse(g_appearance.spectrumResponse);}
    else if(command>=kMenuHeightBase&&command<kMenuHeightBase+kSpectrumHeights.size())g_appearance.spectrumMaxHeight=kSpectrumHeights[command-kMenuHeightBase];
    else if(command>=kMenuFadeBase&&command<kMenuFadeBase+kSpectrumFades.size())g_appearance.spectrumFade=kSpectrumFades[command-kMenuFadeBase];
    else if(command>=kMenuHueBase&&command<kMenuHueBase+kHueSpeeds.size())g_appearance.spectrumHueSpeed=kHueSpeeds[command-kMenuHueBase];
    else switch(command)
    {
      case kMenuCompact:g_appearance.compact=!g_appearance.compact;resizeWindow(hwnd);break;
      case kMenuDynamicAccent:g_appearance.dynamicAccent=!g_appearance.dynamicAccent;break;
      case kMenuArtworkBackground:g_appearance.artworkBackground=!g_appearance.artworkBackground;break;
      case kMenuOverkill:g_appearance.overkill=!g_appearance.overkill;resizeWindow(hwnd);break;
      case kMenuBanner:g_appearance.showBanner=!g_appearance.showBanner;break;
      case kMenuIgnoreParenthetical:g_appearance.ignoreParenthetical=!g_appearance.ignoreParenthetical;break;
      case kMenuSpectrumChroma:g_appearance.spectrumChroma=!g_appearance.spectrumChroma;break;
      case kMenuSpectrumCurves:g_appearance.spectrumCurves=!g_appearance.spectrumCurves;break;
      case kMenuArtworkSpectrum:g_appearance.artworkSpectrum=!g_appearance.artworkSpectrum;break;
      case kMenuDiscordRpc:g_discordSettings.enabled=!g_discordSettings.enabled;if(!g_discordSettings.enabled)g_discordRpc.disconnect();break;
      case kMenuWindowed:g_windowedMode=!g_windowedMode;applyWindowMode(hwnd);break;
      case kMenuAccent:{CHOOSECOLORW cc{sizeof(cc)};static COLORREF custom[16]{};cc.hwndOwner=hwnd;cc.rgbResult=RGB((g_appearance.accentRgb>>16)&255,(g_appearance.accentRgb>>8)&255,g_appearance.accentRgb&255);cc.lpCustColors=custom;cc.Flags=CC_FULLOPEN|CC_RGBINIT;if(ChooseColorW(&cc))g_appearance.accentRgb=(GetRValue(cc.rgbResult)<<16)|(GetGValue(cc.rgbResult)<<8)|GetBValue(cc.rgbResult);break;}
      case kMenuFont:g_settingsWindow.chooseFont();break;
      case kMenuShortcut:openSettings(hwnd);g_recording=true;g_recorded.clear();ZeroMemory(g_scanWasDown,sizeof(g_scanWasDown));updateComboName();break;
      default:return false;
    }
    saveSettings();g_settingsWindow.refresh();InvalidateRect(hwnd,nullptr,FALSE);return true;
  }
  void rebuild(HWND hwnd,UINT w,UINT h){if(!g_swap||!w||!h)return;g_context->ClearState();g_overlay.reset();if(SUCCEEDED(g_swap->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,0))){g_overlay=std::make_unique<mo::IntegrationContext>(mo::Backend::D3D11);g_overlay->initialize({hwnd,g_device.Get(),g_swap.Get(),nullptr});}}
  void scanRecording(){bool any=false;for(int vk=8;vk<255;++vk){if(vk==VK_LBUTTON||vk==VK_RBUTTON||vk==VK_MBUTTON)continue;bool down=(GetAsyncKeyState(vk)&0x8000)!=0;if(down&&!g_scanWasDown[vk]&&std::find(g_recorded.begin(),g_recorded.end(),vk)==g_recorded.end())g_recorded.push_back(vk);g_scanWasDown[vk]=down;if(down)any=true;}if(!any&&!g_recorded.empty()){g_combo=g_recorded;g_recorded.clear();g_recording=false;updateComboName();g_settingsWindow.refreshShortcut();saveSettings();}}
  void scanToggle(HWND hwnd){if(g_windowedMode)return;bool down=comboDown();if(down&&!g_comboWasDown){ULONGLONG now=GetTickCount64();if(g_lastTap&&now-g_lastTap<=kInteractionDoubleTapMs){g_interactive=!g_interactive;g_lastTap=0;applyInteraction(hwnd);}else g_lastTap=now;}g_comboWasDown=down;if(g_lastTap&&GetTickCount64()-g_lastTap>kInteractionDoubleTapMs)g_lastTap=0;}

  bool handleMediaControlClick(HWND hwnd,int x,int y)
  {
    if(!g_appearance.interactive)return false;
    RECT client{};GetClientRect(hwnd,&client);
    constexpr int button=28,gap=5,count=5;
    constexpr int total=button*count+gap*(count-1);
    const int left=std::max(8,static_cast<int>(client.right)-total-14);
    const int top=std::max(8,static_cast<int>(client.bottom)-38);
    if(y<top||y>=top+button||x<left||x>=left+total)return false;
    const int stride=button+gap;
    const int offset=x-left;
    if(offset%stride>=button)return true;
    const int index=offset/stride;
    const auto track=g_state.snapshot();
    switch(index)
    {
      case 0:if(track.canPrevious)g_media.previous();break;
      case 1:if(track.canPlayPause)g_media.togglePlayPause();break;
      case 2:if(track.canNext)g_media.next();break;
      case 3:if(track.canShuffle)g_media.toggleShuffle();break;
      case 4:if(track.canRepeat)g_media.cycleRepeat();break;
      default:return false;
    }
    InvalidateRect(hwnd,nullptr,FALSE);return true;
  }

  LRESULT CALLBACK wndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){switch(msg)
  {
      case WM_COMMAND:if(handleTraySetting(hwnd,LOWORD(wp)))return 0;switch(LOWORD(wp)){case kMenuSettings:openSettings(hwnd);break;case kMenuToggle:if(!g_windowedMode){g_interactive=!g_interactive;applyInteraction(hwnd);InvalidateRect(hwnd,nullptr,FALSE);}break;case kMenuExit:DestroyWindow(hwnd);break;}return 0;
      case kTrayMessage:if(LOWORD(lp)==WM_LBUTTONDBLCLK){openSettings(hwnd);return 0;}if(LOWORD(lp)==WM_RBUTTONUP||LOWORD(lp)==WM_CONTEXTMENU){showTrayMenu(hwnd);return 0;}return 0;
      case WM_NCHITTEST:if(g_windowedMode)return DefWindowProcW(hwnd,msg,wp,lp);return g_interactive?HTCLIENT:HTTRANSPARENT;
      case WM_TIMER:{if(g_recording)scanRecording();else scanToggle(hwnd);ULONGLONG now=GetTickCount64();if(!g_lastMediaPoll||now-g_lastMediaPoll>=250){g_lastMediaPoll=now;g_media.poll();auto track=g_state.snapshot();g_discordRpc.update(track,g_discordSettings);}if(g_appearance.overkill&&now-g_lastSpectrumCheck>=1000){g_lastSpectrumCheck=now;auto track=g_state.snapshot();g_spectrum.ensureForApp(track.sourceAppId);positionOverkill(hwnd);}else if(!g_appearance.overkill&&!g_spectrum.activeAppId().empty())g_spectrum.stop();InvalidateRect(hwnd,nullptr,FALSE);return 0;}
      case WM_SIZE:if(wp!=SIZE_MINIMIZED)rebuild(hwnd,LOWORD(lp),HIWORD(lp));return 0;
      case WM_MOVING:{if(g_windowedMode)return DefWindowProcW(hwnd,msg,wp,lp);auto* moving=reinterpret_cast<RECT*>(lp);HMONITOR monitor=MonitorFromRect(moving,MONITOR_DEFAULTTONEAREST);MONITORINFO info{sizeof(info)};if(GetMonitorInfoW(monitor,&info)){LONG width=moving->right-moving->left,height=moving->bottom-moving->top;LONG x=width>=info.rcWork.right-info.rcWork.left?info.rcWork.left:std::clamp(moving->left,info.rcWork.left,info.rcWork.right-width);LONG y=height>=info.rcWork.bottom-info.rcWork.top?info.rcWork.top:std::clamp(moving->top,info.rcWork.top,info.rcWork.bottom-height);moving->left=x;moving->top=y;moving->right=x+width;moving->bottom=y+height;}return TRUE;}
      case WM_EXITSIZEMOVE:if(!g_windowedMode){if(g_appearance.overkill)positionOverkill(hwnd);else constrainToMonitor(hwnd);}return 0;
      case WM_GETMINMAXINFO:if(g_windowedMode){auto* info=reinterpret_cast<MINMAXINFO*>(lp);info->ptMinTrackSize={360,140};return 0;}break;
      case WM_LBUTTONDOWN:{int x=GET_X_LPARAM(lp),y=GET_Y_LPARAM(lp);if(handleMediaControlClick(hwnd,x,y))return 0;if(g_windowedMode){SetForegroundWindow(hwnd);ReleaseCapture();SendMessageW(hwnd,WM_NCLBUTTONDOWN,HTCAPTION,0);return 0;}if(!g_interactive)return 0;RECT r{};GetClientRect(hwnd,&r);
        if(g_appearance.settingsOpen&&y>=150&&y<=184&&x>=130){g_appearance.opacity=.1f+.9f*std::clamp(float(x-140)/float(std::max<LONG>(1L,r.right-164)),0.f,1.f);applyOpacity(hwnd);saveSettings();return 0;}
        if(g_appearance.settingsOpen&&y>=185&&y<=218){g_appearance.compact=!g_appearance.compact;saveSettings();InvalidateRect(hwnd,nullptr,FALSE);return 0;}
        if(g_appearance.settingsOpen&&y>=218&&y<=252){CHOOSECOLORW cc{sizeof(cc)};static COLORREF custom[16]{};cc.hwndOwner=hwnd;cc.rgbResult=RGB((g_appearance.accentRgb>>16)&255,(g_appearance.accentRgb>>8)&255,g_appearance.accentRgb&255);cc.lpCustColors=custom;cc.Flags=CC_FULLOPEN|CC_RGBINIT;if(ChooseColorW(&cc)){g_appearance.accentRgb=(GetRValue(cc.rgbResult)<<16)|(GetGValue(cc.rgbResult)<<8)|GetBValue(cc.rgbResult);saveSettings();}return 0;}
        if(g_appearance.settingsOpen&&y>=252&&y<=286){g_appearance.dynamicAccent=!g_appearance.dynamicAccent;saveSettings();return 0;}
        if(g_appearance.settingsOpen&&y>=286&&y<=320){g_appearance.artworkBackground=!g_appearance.artworkBackground;saveSettings();return 0;}
        if(g_appearance.settingsOpen&&y>=320&&y<=354){g_recording=true;g_recorded.clear();ZeroMemory(g_scanWasDown,sizeof(g_scanWasDown));updateComboName();return 0;}
        if(g_appearance.settingsOpen&&y>=354&&y<=390){g_backend=g_backend==9?10:g_backend==10?11:g_backend==11?12:9;updateBackendName();saveSettings();return 0;}
        if(g_appearance.settingsOpen&&y>=390&&y<=424){g_appearance.overkill=!g_appearance.overkill;saveSettings();InvalidateRect(hwnd,nullptr,FALSE);return 0;}
        if(g_appearance.settingsOpen&&y>=424&&y<=460){g_appearance.corner=(g_appearance.corner+1)%4;updateCornerName();saveSettings();InvalidateRect(hwnd,nullptr,FALSE);return 0;}
        if(g_appearance.settingsOpen&&y>=460&&y<=496){g_appearance.showBanner=!g_appearance.showBanner;saveSettings();InvalidateRect(hwnd,nullptr,FALSE);return 0;}
        if(g_appearance.settingsOpen&&y>=496&&y<=532){g_appearance.ignoreParenthetical=!g_appearance.ignoreParenthetical;saveSettings();InvalidateRect(hwnd,nullptr,FALSE);return 0;}
        if(g_appearance.overkill)return 0;
        ReleaseCapture();SendMessageW(hwnd,WM_NCLBUTTONDOWN,HTCAPTION,0);return 0;}
        case WM_MOUSEMOVE:if(g_interactive&&g_appearance.settingsOpen&&(wp&MK_LBUTTON)){RECT r{};GetClientRect(hwnd,&r);int x=GET_X_LPARAM(lp),y=GET_Y_LPARAM(lp);if(y>=150&&y<=184){g_appearance.opacity=.1f+.9f*std::clamp(float(x-140)/float(std::max<LONG>(1L,r.right-164)),0.f,1.f);applyOpacity(hwnd);InvalidateRect(hwnd,nullptr,FALSE);}}return 0;
        case WM_LBUTTONUP:saveSettings();return 0;
        case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(hwnd,&ps);RECT r;GetClientRect(hwnd,&r);if(g_overlay){g_appearance.logoData=g_bannerData;g_appearance.logoDataSize=g_bannerSize;mo::FrameView f{static_cast<UINT>(r.right),static_cast<UINT>(r.bottom),96.f,g_state.snapshot(),g_appearance,g_spectrum.snapshot()};g_overlay->render(f);g_swap->Present(1,0);}EndPaint(hwnd,&ps);return 0;}
        case WM_DESTROY:g_discordRpc.disconnect();g_spectrum.stop();g_media.stop();saveSettings();Shell_NotifyIconW(NIM_DELETE,&g_tray);PostQuitMessage(0);return 0;}return DefWindowProcW(hwnd,msg,wp,lp);}
        }

        int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int)
        {
          winrt::init_apartment(winrt::apartment_type::multi_threaded);SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
          loadSettings();loadFontSetting();loadSpectrumSettings();loadDiscordSettings();loadWindowedSetting();updateBackendName();updateCornerName();
          if(HRSRC resource=FindResourceW(instance,MAKEINTRESOURCEW(IDR_BANNER_PNG),RT_RCDATA)){if(HGLOBAL data=LoadResource(instance,resource)){g_bannerData=static_cast<const std::uint8_t*>(LockResource(data));g_bannerSize=SizeofResource(instance,resource);}}
          HICON icon=LoadIconW(instance,MAKEINTRESOURCEW(IDI_MUSICOVERLAY));WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.style=CS_HREDRAW|CS_VREDRAW;wc.lpfnWndProc=wndProc;wc.hInstance=instance;wc.hIcon=icon;wc.hIconSm=icon;wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));wc.lpszClassName=kClassName;RegisterClassExW(&wc);
          int width=g_windowedMode?640:collapsedWidth(),height=g_windowedMode?240:collapsedHeight();DWORD style=g_windowedMode?WS_OVERLAPPEDWINDOW:WS_POPUP;DWORD exStyle=g_windowedMode?(WS_EX_APPWINDOW|WS_EX_NOREDIRECTIONBITMAP):(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOREDIRECTIONBITMAP|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE);
          HWND hwnd=CreateWindowExW(exStyle,kClassName,L"Nihon's Music Framework",style,GetSystemMetrics(SM_CXSCREEN)-width-28,28,width,height,nullptr,nullptr,instance,nullptr);if(!hwnd||!createDevice(hwnd))return 1;
          mo::standalone::SettingsBindings bindings{&g_appearance,&g_discordSettings,&g_backend,&g_windowedMode,&g_fontFamily,hwnd,settingsApply,settingsRecordShortcut};g_settingsWindow.initialize(instance,icon,bindings);
          applyWindowMode(hwnd);ShowWindow(hwnd,g_windowedMode?SW_SHOW:SW_SHOWNOACTIVATE);addTrayIcon(hwnd,icon);SetTimer(hwnd,kTimer,8,nullptr);g_media.start();MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return static_cast<int>(msg.wParam);
        }
