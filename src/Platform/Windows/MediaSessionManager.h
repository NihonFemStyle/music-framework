#pragma once
#include <MusicOverlay/Core/OverlayState.h>

#if defined(MUSICOVERLAY_MEDIA_ABI)
// Raw Windows Runtime ABI implementation used by the no-CRT build. It talks to
// Global System Media Transport Controls through COM interfaces on a worker
// thread and never uses C++/WinRT, exceptions, or coroutines.
namespace mo::win
{
class MediaSessionManager
{
  public:
  explicit MediaSessionManager(OverlayState& state) : state_(state) {}
  ~MediaSessionManager();
  MediaSessionManager(const MediaSessionManager&) = delete;
  MediaSessionManager& operator=(const MediaSessionManager&) = delete;
  void start();
  void poll();
  void previous();
  void togglePlayPause();
  void next();
  void toggleShuffle();
  void cycleRepeat();
  void stop() noexcept;
  private:
  static unsigned long __stdcall threadEntry(void* context);
  void run();
  void queue(long command);
  OverlayState& state_;
  void* thread_{};
  void* wake_{};
  volatile long stopped_{};
  volatile long commands_{};
  };
  }
#else
#include <atomic>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
namespace mo::win
{
class MediaSessionManager
{
  public:
  explicit MediaSessionManager(OverlayState& state) : state_(state) {}
  winrt::Windows::Foundation::IAsyncAction start();
  void poll() { refresh(); }
  void previous();
  void togglePlayPause();
  void next();
  void toggleShuffle();
  void cycleRepeat();
  void stop() noexcept { stopped_.store(true); }
  private:
  winrt::Windows::Foundation::IAsyncAction refresh();
  enum class Command { Previous, PlayPause, Next, Shuffle, Repeat };
  winrt::fire_and_forget execute(Command command);
  OverlayState& state_;
  std::atomic_bool stopped_{};
  winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager manager_{nullptr};
  winrt::event_token sessionsToken_{};
  winrt::event_token currentToken_{};
  };
  }
#endif
