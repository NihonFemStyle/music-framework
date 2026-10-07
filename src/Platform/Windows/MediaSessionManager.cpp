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
        auto controls=playback.Controls();
        track.canPrevious=controls.IsPreviousEnabled();track.canPlayPause=controls.IsPlayEnabled()||controls.IsPauseEnabled();track.canNext=controls.IsNextEnabled();track.canShuffle=controls.IsShuffleEnabled();track.canRepeat=controls.IsRepeatEnabled();
        if(auto shuffle=playback.IsShuffleActive())track.shuffleActive=shuffle.Value();
        if(auto repeat=playback.AutoRepeatMode())track.repeatMode=repeat.Value()==Windows::Media::MediaPlaybackAutoRepeatMode::Track?1:repeat.Value()==Windows::Media::MediaPlaybackAutoRepeatMode::List?2:0;
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

void MediaSessionManager::previous(){execute(Command::Previous);}void MediaSessionManager::togglePlayPause(){execute(Command::PlayPause);}void MediaSessionManager::next(){execute(Command::Next);}void MediaSessionManager::toggleShuffle(){execute(Command::Shuffle);}void MediaSessionManager::cycleRepeat(){execute(Command::Repeat);}

winrt::fire_and_forget MediaSessionManager::execute(Command command)
{
  try
  {
    if(!manager_||stopped_)co_return;auto session=manager_.GetCurrentSession();if(!session)co_return;auto playback=session.GetPlaybackInfo();auto controls=playback.Controls();
    switch(command)
    {
      case Command::Previous:if(controls.IsPreviousEnabled())co_await session.TrySkipPreviousAsync();break;
      case Command::PlayPause:if(playback.PlaybackStatus()==GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing){if(controls.IsPauseEnabled())co_await session.TryPauseAsync();}else if(controls.IsPlayEnabled())co_await session.TryPlayAsync();break;
      case Command::Next:if(controls.IsNextEnabled())co_await session.TrySkipNextAsync();break;
      case Command::Shuffle:if(controls.IsShuffleEnabled()){bool active=false;if(auto value=playback.IsShuffleActive())active=value.Value();co_await session.TryChangeShuffleActiveAsync(!active);}break;
      case Command::Repeat:if(controls.IsRepeatEnabled()){auto mode=Windows::Media::MediaPlaybackAutoRepeatMode::None;if(auto value=playback.AutoRepeatMode())mode=value.Value();auto nextMode=mode==Windows::Media::MediaPlaybackAutoRepeatMode::None?Windows::Media::MediaPlaybackAutoRepeatMode::List:mode==Windows::Media::MediaPlaybackAutoRepeatMode::List?Windows::Media::MediaPlaybackAutoRepeatMode::Track:Windows::Media::MediaPlaybackAutoRepeatMode::None;co_await session.TryChangeAutoRepeatModeAsync(nextMode);}break;
    }
    co_await refresh();
  }catch(...){ }
}
            }
