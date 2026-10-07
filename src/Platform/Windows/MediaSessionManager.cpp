#include <winrt/Windows.Storage.Streams.h>

#include "Core/MetadataFormatter.h"
#include "MediaSessionManager.h"
using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Storage::Streams;

namespace mo::win
{
winrt::Windows::Foundation::IAsyncAction MediaSessionManager::start()
{
  manager_ = co_await GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
  if (stopped_) co_return;
  sessionsToken_ = manager_.SessionsChanged([this](auto&&, auto&&) { refresh(); });
  currentToken_ = manager_.CurrentSessionChanged([this](auto&&, auto&&) { refresh(); });
  co_await refresh();
  }

  winrt::Windows::Foundation::IAsyncAction MediaSessionManager::refresh()
  {
    if (!manager_ || stopped_) co_return;
    auto session = manager_.GetCurrentSession();
    TrackInfo track;
    if (session)
    {
      try
      {
        track.sourceAppId = session.SourceAppUserModelId().c_str();
        auto props = co_await session.TryGetMediaPropertiesAsync();
        track.title = props.Title().c_str();
        track.artist = props.Artist().c_str();
        track.album = props.AlbumTitle().c_str();
        auto playback = session.GetPlaybackInfo();
        track.playing = playback.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
        auto timeline = session.GetTimelineProperties();
        track.position100ns = timeline.Position().count();
        track.duration100ns = (timeline.EndTime() - timeline.StartTime()).count();
        if (auto thumb = props.Thumbnail())
        {
          auto stream = co_await thumb.OpenReadAsync();
          auto size = static_cast<std::uint32_t>(stream.Size());
          Buffer buffer(size);
          auto read = co_await stream.ReadAsync(buffer, size, InputStreamOptions::None);
          track.artwork.resize(read.Length());
          DataReader reader = DataReader::FromBuffer(read);
          reader.ReadBytes(track.artwork);
          }
          } catch (...) { }
            }
            formatTrackMetadata(track);
            track.revision = state_.snapshot().revision + 1;
            state_.update(std::move(track));
            }
            }
