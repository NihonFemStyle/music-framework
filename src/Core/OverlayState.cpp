#include <MusicOverlay/Core/OverlayState.h>
namespace mo {
void OverlayState::update(TrackInfo value) { std::scoped_lock lock(mutex_); track_ = std::move(value); }
TrackInfo OverlayState::snapshot() const { std::scoped_lock lock(mutex_); return track_; }
}

