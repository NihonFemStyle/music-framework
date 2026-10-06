# Integration guide

The public integration surface is declared in `include/MusicOverlay`.

## Basic use

```cpp
#include <MusicOverlay/Integration/MusicOverlay.h>

mo::IntegrationContext overlay{mo::Backend::D3D11};

mo::NativeTarget target{};
target.window = hwnd;
target.device = d3d11Device;
target.swapChain = dxgiSwapChain;

if (!overlay.initialize(target)) {
    // Surface/device combination is unsupported.
}

mo::FrameView frame{};
frame.width = width;
frame.height = height;
frame.dpi = dpi;
frame.track = currentTrack;
frame.appearance.opacity = 0.92f;

overlay.render(frame); // Call before Present.
```

The host retains ownership of every pointer in `NativeTarget` and must keep them alive until it destroys the integration context.

## Backend contracts

### Direct3D 9

Pass an `IDirect3DDevice9*` in `device`. The included path uses `GetBackBuffer`, `GetDC`, and native GDI text, so the surface must support that operation. This is a compatibility path rather than feature parity with the D3D11 painter.

### Direct3D 10

Pass `ID3D10Device*` and `IDXGISwapChain*`. The swap-chain buffer must be compatible with the Direct2D DXGI-surface render target used by the painter.

### Direct3D 11

Pass `ID3D11Device*` and `IDXGISwapChain*`. The device must have BGRA support when it is created. This is the full reference implementation used by the standalone executable.

### Direct3D 12

Pass `ID3D12Device*`, `IDXGISwapChain*`, and `ID3D12CommandQueue*`. The current backend validates and stores the binding but intentionally does not record commands. A production host must define command-list ownership, descriptor allocation, backbuffer state transitions, fences, and synchronization before rendering overlay primitives.

## Resize and device loss

Release render-target resources before resizing a swap chain, resize host buffers, and then call `resize` or recreate the integration context. The standalone application recreates its D3D11 painter after `ResizeBuffers`.

## API stability

Version 1.0 establishes the initial public integration API. Additive fields may appear in minor releases, while incompatible public-header or renderer-contract changes are reserved for a new major version. Prefer named assignments over positional aggregate initialization where forward compatibility matters.

