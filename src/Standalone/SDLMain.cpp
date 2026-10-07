#include <MusicOverlay/Core/OverlayState.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <string>
#include <string_view>
#include <thread>
#include <windows.h>

#include "Platform/Windows/MediaSessionManager.h"
#include "Platform/Windows/ProcessSpectrumCapture.h"
namespace
{
std::string utf8(const std::wstring& text){if(text.empty())return {};int size=WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);std::string result(size,'\0');WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),result.data(),size,nullptr,nullptr);return result;}
void text(SDL_Renderer* renderer,const std::wstring& value,float x,float y,float scale,SDL_Color color){auto encoded=utf8(value);if(encoded.empty())return;SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,color.a);SDL_SetRenderScale(renderer,scale,scale);SDL_RenderDebugText(renderer,x/scale,y/scale,encoded.c_str());SDL_SetRenderScale(renderer,1.f,1.f);}
const char* selectedDriver(int argc,char** argv){for(int i=1;i+1<argc;++i)if(std::string_view(argv[i])=="--renderer")return std::string_view(argv[i+1])=="vulkan"?"vulkan":"opengl";return "opengl";}
void applyOverlayStyles(SDL_Window* window){auto properties=SDL_GetWindowProperties(window);auto hwnd=static_cast<HWND>(SDL_GetPointerProperty(properties,SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr));if(!hwnd)return;LONG_PTR styles=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);SetWindowLongPtrW(hwnd,GWL_EXSTYLE,styles|WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE);SetLayeredWindowAttributes(hwnd,0,255,LWA_ALPHA);}
}

int main(int argc,char** argv)
{
  winrt::init_apartment(winrt::apartment_type::multi_threaded);
  const char* driver=selectedDriver(argc,argv);SDL_SetHint(SDL_HINT_RENDER_DRIVER,driver);
  if(!SDL_Init(SDL_INIT_VIDEO))return 1;
  SDL_Window* window=SDL_CreateWindow("Nihon's Music Framework — SDL",520,180,SDL_WINDOW_TRANSPARENT|SDL_WINDOW_BORDERLESS|SDL_WINDOW_ALWAYS_ON_TOP|SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if(!window){SDL_Quit();return 2;}SDL_SetWindowPosition(window,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED);applyOverlayStyles(window);
  SDL_Renderer* renderer=SDL_CreateRenderer(window,nullptr);if(!renderer){SDL_DestroyWindow(window);SDL_Quit();return 3;}
  const char* actual=SDL_GetRendererName(renderer);if(!actual||std::string_view(actual).find(driver)==std::string_view::npos){SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 4;}
  mo::OverlayState state;mo::win::MediaSessionManager media(state);mo::win::ProcessSpectrumCapture spectrum;media.start();std::wstring capturedApp;bool running=true;
  while(running){SDL_Event event{};while(SDL_PollEvent(&event))if(event.type==SDL_EVENT_QUIT)running=false;media.poll();auto track=state.snapshot();if(track.sourceAppId!=capturedApp){spectrum.startForApp(track.sourceAppId);capturedApp=track.sourceAppId;}
    int width{},height{};SDL_GetRenderOutputSize(renderer,&width,&height);auto bands=spectrum.snapshot();SDL_SetRenderDrawColor(renderer,0,0,0,0);SDL_RenderClear(renderer);SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);float barWidth=float(width)/float(bands.size());bool top=true;for(std::size_t i=0;i<bands.size();++i){float level=std::clamp(bands[i],0.f,1.f);float barHeight=4.f+level*(float(height)-4.f);SDL_SetRenderDrawColor(renderer,104,213,200,static_cast<Uint8>(65+level*170));SDL_FRect bar{float(i)*barWidth,top?0.f:float(height)-barHeight,std::max(1.f,barWidth-1.f),barHeight};SDL_RenderFillRect(renderer,&bar);}text(renderer,track.title.empty()?L"Nothing playing":track.title,18.f,48.f,2.5f,{244,247,251,255});text(renderer,track.artist.empty()?L"Windows media session":track.artist,20.f,92.f,1.5f,{190,198,210,255});SDL_RenderPresent(renderer);std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    spectrum.stop();media.stop();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
    }
