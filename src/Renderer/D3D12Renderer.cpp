#include "Renderers.h"
namespace mo {
bool D3D12Renderer::initialize(const NativeTarget& t) {
  device_=static_cast<ID3D12Device*>(t.device); swap_=static_cast<IDXGISwapChain*>(t.swapChain); queue_=static_cast<ID3D12CommandQueue*>(t.commandQueue);
  return device_ && swap_ && queue_;
}
void D3D12Renderer::render(const FrameView&) {
  // D3D12 hosts own command-list state. The portable API validates/binds here;
  // hosts render the supplied UI data in their command list. See README integration contract.
}
}
