#pragma once
#include <MusicOverlay/Core/TrackInfo.h>

namespace mo {
// Thread-safe snapshot holder. Uses a Win32 slim reader/writer lock instead of
// std::mutex so the same code links in both the normal and the no-CRT builds.
class OverlayState {
public:
  OverlayState() = default;
  OverlayState(const OverlayState&) = delete;
  OverlayState& operator=(const OverlayState&) = delete;
  void update(TrackInfo value);
  [[nodiscard]] TrackInfo snapshot() const;
private:
  mutable void* lock_{}; // SRWLOCK storage; SRWLOCK_INIT is a null pointer.
  TrackInfo track_;
};
}
