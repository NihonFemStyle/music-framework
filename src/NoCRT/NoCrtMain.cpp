#include <windows.h>

// Minimal compiler-required memory primitives. Optimized MSVC builds may lower
// aggregate initialization and copies to these symbols even when the source
// does not call the CRT directly.
#pragma function(memset, memcpy)
#pragma optimize("", off)
extern "C" void* __cdecl memset(void* destination, int value, size_t count) {
  auto* output = static_cast<volatile unsigned char*>(destination);
  while (count--) *output++ = static_cast<unsigned char>(value);
  return destination;
}

extern "C" void* __cdecl memcpy(void* destination, const void* source, size_t count) {
  auto* output = static_cast<volatile unsigned char*>(destination);
  auto* input = static_cast<const volatile unsigned char*>(source);
  while (count--) *output++ = *input++;
  return destination;
}
#pragma optimize("", on)

static LRESULT CALLBACK NoCrtProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == WM_NCHITTEST) return (GetAsyncKeyState(VK_SHIFT)&0x8000) ? HTCLIENT : HTTRANSPARENT;
  if (msg == WM_PAINT) {
    PAINTSTRUCT ps; HDC dc=BeginPaint(hwnd,&ps); RECT r; GetClientRect(hwnd,&r);
    HBRUSH brush=CreateSolidBrush(RGB(17,21,29)); FillRect(dc,&r,brush); DeleteObject(brush);
    SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(244,247,251));
    RECT text{20,18,r.right-20,r.bottom-12}; DrawTextW(dc,L"MusicOverlay NOCRT\nMedia discovery and rich rendering are disabled",-1,&text,DT_LEFT|DT_NOPREFIX);
    EndPaint(hwnd,&ps); return 0;
  }
  if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
  return DefWindowProcW(hwnd,msg,wp,lp);
}

extern "C" void WINAPI WinMainCRTStartup() {
  HINSTANCE instance=GetModuleHandleW(nullptr);
  WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc); wc.hInstance=instance; wc.lpfnWndProc=NoCrtProc; wc.lpszClassName=L"MusicOverlayNoCRT"; wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));
  RegisterClassExW(&wc);
  HWND hwnd=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE,wc.lpszClassName,L"MusicOverlay NOCRT",WS_POPUP,40,40,430,100,nullptr,nullptr,instance,nullptr);
  SetLayeredWindowAttributes(hwnd,0,235,LWA_ALPHA); ShowWindow(hwnd,SW_SHOWNOACTIVATE);
  MSG msg{}; while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);} ExitProcess((UINT)msg.wParam);
}

