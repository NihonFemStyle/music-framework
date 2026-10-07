#pragma once
#include <MusicOverlay/Core/OverlayState.h>
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
  void stop() noexcept { stopped_.store(true); }
  private:
  winrt::Windows::Foundation::IAsyncAction refresh();
  OverlayState& state_;
  std::atomic_bool stopped_{};
  winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager manager_{nullptr};
  winrt::event_token sessionsToken_{};
  winrt::event_token currentToken_{};
  };
  }
