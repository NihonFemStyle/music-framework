#include <MusicOverlay/Core/OverlayState.h>
#include <utility>
#include <windows.h>
namespace mo
{
static_assert(sizeof(SRWLOCK) == sizeof(void*), "SRWLOCK must fit the reserved lock storage");
void OverlayState::update(TrackInfo value)
{
  auto* lock = reinterpret_cast<PSRWLOCK>(&lock_);
  AcquireSRWLockExclusive(lock);
  track_ = std::move(value);
  ReleaseSRWLockExclusive(lock);
}
TrackInfo OverlayState::snapshot() const
{
  auto* lock = reinterpret_cast<PSRWLOCK>(&lock_);
  AcquireSRWLockShared(lock);
  TrackInfo copy = track_;
  ReleaseSRWLockShared(lock);
  return copy;
}
}
