#pragma once
#include <MusicOverlay/Core/TrackInfo.h>
#include <mutex>

namespace mo {
class OverlayState {
public:
  void update(TrackInfo value);
  [[nodiscard]] TrackInfo snapshot() const;
private:
  mutable std::mutex mutex_;
  TrackInfo track_;
};
}

