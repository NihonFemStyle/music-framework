#include "Renderers.h"
namespace mo {
std::unique_ptr<IRenderer> createRenderer(Backend b) {
  switch (b) {
    case Backend::D3D9: return std::make_unique<D3D9Renderer>();
    case Backend::D3D10: return std::make_unique<D3D10Renderer>();
    case Backend::D3D11: return std::make_unique<D3D11Renderer>();
    case Backend::D3D12: return std::make_unique<D3D12Renderer>();
  }
  return {};
}
}

