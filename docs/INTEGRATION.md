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
frame.appearance.fontFamily = L"Segoe UI Variable Display";

overlay.render(frame); // Call before Present.
```

`fontFamily` may name any installed DirectWrite font family. The painter retains automatic fitting for long titles and falls back through DirectWrite when a requested family is unavailable.

The host retains ownership of every pointer in `NativeTarget` and must keep them alive until it destroys the integration context.

## Backend contracts

### Direct3D 9

Pass an `IDirect3DDevice9*` (or `IDirect3DDevice9Ex*`) in `device`. The renderer draws the full overlay. The shared painter renders into a CPU buffer, which is uploaded to an `A8R8G8B8` texture and drawn as a premultiplied-alpha quad at the top-left of the render target.

- Call `render` either inside the host's `BeginScene`/`EndScene` pair or outside it. The renderer detects which and opens its own scene only when needed.
- All render, texture-stage, sampler, shader, and FVF state is captured and restored with a `D3DSBT_ALL` state block.
- On a classic device the texture lives in the managed pool. On a 9Ex device it is a dynamic default-pool texture, so call `releaseTargets()` before `Reset`/`ResetEx`.

### Direct3D 10

Pass `ID3D10Device*` (a 10.1 device created with `D3D10_CREATE_DEVICE_BGRA_SUPPORT`) and `IDXGISwapChain*`. The back buffer must be `B8G8R8A8_UNORM` or `R8G8B8A8_UNORM`. The renderer draws over the existing contents; it does not clear.

### Direct3D 11

Pass `ID3D11Device*` and `IDXGISwapChain*`. The device must have BGRA support when it is created. This is the full reference implementation used by the standalone executable.

### Direct3D 12

Pass `ID3D12Device*`, the `IDXGISwapChain*` (it must support `IDXGISwapChain3`), and the `ID3D12CommandQueue*` that presents it. The renderer draws the full overlay through D3D11On12 on that queue. It never touches the host's command lists, allocators, descriptor heaps, or fences.

The contract:

1. **Ordering.** Call `render(frame)` once per frame, after the host has called `ExecuteCommandLists` for its own work on the same queue, and before `Present`. The overlay's GPU work is submitted to the queue during `render`, so queue order guarantees it lands on top of the host's frame.
2. **Resource state.** When `render` is called, the current back buffer must be in `NativeTarget::resourceState`. It is returned in the same state. The default `0` is `D3D12_RESOURCE_STATE_PRESENT`, which matches a host that transitions to `PRESENT` at the end of its command list. A host that presents from another state, or calls `render` while the buffer is still a render target, sets `resourceState = D3D12_RESOURCE_STATE_RENDER_TARGET`.
3. **Formats.** The back buffers must be `B8G8R8A8_UNORM` or `R8G8B8A8_UNORM`. Direct2D cannot target `_SRGB` formats.
4. **Resize.** Call `releaseTargets()` before `ResizeBuffers`. It releases the wrapped back buffers and flushes D3D11On12. Call `resize(w, h)` afterwards. As with any `ResizeBuffers` call, the host must first wait until the GPU has finished with the old buffers.
5. **Threading.** Call the context from one thread at a time. It uses the queue only from inside `render`.

The standalone application's Direct3D 12 host (`src/Standalone/GraphicsHosts.cpp`) follows this contract exactly and is a working reference.

## Resize and device loss

Call `IntegrationContext::releaseTargets()` before `IDXGISwapChain::ResizeBuffers` or `IDirect3DDevice9::Reset`. Then resize the host buffers and call `resize(width, height)`. Destroying and recreating the context also works.

## API stability

Version 1.0 establishes the initial public integration API. Additive fields may appear in minor releases, while incompatible public-header or renderer-contract changes are reserved for a new major version. Prefer named assignments over positional aggregate initialization where forward compatibility matters.

