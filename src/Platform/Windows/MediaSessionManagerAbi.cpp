#include "MediaSessionManager.h"

#if defined(MUSICOVERLAY_MEDIA_ABI)
#include <windows.h>
#include <roapi.h>
#include <shcore.h>
#include <winstring.h>
#include <windows.foundation.h>
#include <windows.media.control.h>
#include <windows.storage.streams.h>
#include <wrl/client.h>

#include <string>
#include <utility>

#include "Core/MetadataFormatter.h"

using Microsoft::WRL::ComPtr;
namespace abiF = ABI::Windows::Foundation;
namespace abiC = ABI::Windows::Media::Control;
namespace abiM = ABI::Windows::Media;
namespace abiS = ABI::Windows::Storage::Streams;

namespace mo::win
{
namespace
{
enum : long { CommandPrevious = 1, CommandPlayPause = 2, CommandNext = 4, CommandShuffle = 8, CommandRepeat = 16, CommandRefresh = 32 };
constexpr DWORD kPollIntervalMs = 250;
constexpr DWORD kOperationTimeoutMs = 3000;
constexpr ULONGLONG kArtworkRefreshMs = 2000;
constexpr ULONGLONG kMaxArtworkBytes = 32ull * 1024ull * 1024ull;

// Polls an IAsyncOperation until it leaves the Started state. Polling keeps the
// implementation free of hand-written completion-handler COM classes.
template <class Operation>
bool waitFor(Operation* operation, DWORD timeoutMs, volatile long* stopped)
{
  if (!operation) return false;
  ComPtr<abiF::IAsyncInfo> info;
  if (FAILED(operation->QueryInterface(IID_PPV_ARGS(&info)))) return false;
  const ULONGLONG started = GetTickCount64();
  for (;;)
  {
    abiF::AsyncStatus status = abiF::AsyncStatus::Started;
    if (FAILED(info->get_Status(&status))) return false;
    if (status == abiF::AsyncStatus::Completed) return true;
    if (status != abiF::AsyncStatus::Started) return false;
    if (*stopped || GetTickCount64() - started > timeoutMs) { info->Cancel(); return false; }
    Sleep(1);
  }
}

std::wstring takeString(HSTRING value)
{
  std::wstring result;
  if (!value) return result;
  UINT32 length = 0;
  const wchar_t* raw = WindowsGetStringRawBuffer(value, &length);
  if (raw && length) result.assign(raw, length);
  WindowsDeleteString(value);
  return result;
}

bool runBoolOperation(ComPtr<abiF::IAsyncOperation<bool>>& operation, HRESULT started, volatile long* stopped)
{
  return SUCCEEDED(started) && waitFor(operation.Get(), kOperationTimeoutMs, stopped);
}

struct Worker
{
  OverlayState& state;
  volatile long* stopped;
  ComPtr<abiC::IGlobalSystemMediaTransportControlsSessionManager> manager;
  std::wstring artworkKey;
  std::vector<std::uint8_t> artwork;
  ULONGLONG artworkTick{};

  bool connect()
  {
    HSTRING_HEADER header{};
    HSTRING className{};
    const wchar_t* name = RuntimeClass_Windows_Media_Control_GlobalSystemMediaTransportControlsSessionManager;
    if (FAILED(WindowsCreateStringReference(name, static_cast<UINT32>(wcslen(name)), &header, &className))) return false;
    ComPtr<abiC::IGlobalSystemMediaTransportControlsSessionManagerStatics> statics;
    if (FAILED(RoGetActivationFactory(className, IID_PPV_ARGS(&statics)))) return false;
    ComPtr<abiF::IAsyncOperation<abiC::GlobalSystemMediaTransportControlsSessionManager*>> operation;
    if (FAILED(statics->RequestAsync(&operation)) || !waitFor(operation.Get(), 10000, stopped)) return false;
    return SUCCEEDED(operation->GetResults(&manager)) && manager;
  }

  ComPtr<abiC::IGlobalSystemMediaTransportControlsSession> currentSession()
  {
    ComPtr<abiC::IGlobalSystemMediaTransportControlsSession> session;
    if (manager) manager->GetCurrentSession(&session);
    return session;
  }

  void readArtwork(abiC::IGlobalSystemMediaTransportControlsSessionMediaProperties* properties, std::vector<std::uint8_t>& output)
  {
    ComPtr<abiS::IRandomAccessStreamReference> thumbnail;
    if (FAILED(properties->get_Thumbnail(&thumbnail)) || !thumbnail) return;
    ComPtr<abiF::IAsyncOperation<abiS::IRandomAccessStreamWithContentType*>> operation;
    if (FAILED(thumbnail->OpenReadAsync(&operation)) || !waitFor(operation.Get(), kOperationTimeoutMs, stopped)) return;
    ComPtr<abiS::IRandomAccessStreamWithContentType> stream;
    if (FAILED(operation->GetResults(&stream)) || !stream) return;
    ComPtr<IStream> classic;
    if (FAILED(CreateStreamOverRandomAccessStream(stream.Get(), IID_PPV_ARGS(&classic)))) return;
    STATSTG stat{};
    if (FAILED(classic->Stat(&stat, STATFLAG_NONAME))) return;
    const ULONGLONG size = stat.cbSize.QuadPart;
    if (!size || size > kMaxArtworkBytes) return;
    output.resize(static_cast<std::size_t>(size));
    ULONG total = 0;
    while (total < static_cast<ULONG>(size))
    {
      ULONG read = 0;
      if (FAILED(classic->Read(output.data() + total, static_cast<ULONG>(size) - total, &read)) || !read) break;
      total += read;
    }
    output.resize(total);
  }

  void refresh()
  {
    TrackInfo track;
    auto session = currentSession();
    if (session)
    {
      HSTRING appId{};
      if (SUCCEEDED(session->get_SourceAppUserModelId(&appId))) track.sourceAppId = takeString(appId);

      ComPtr<abiF::IAsyncOperation<abiC::GlobalSystemMediaTransportControlsSessionMediaProperties*>> propertiesOperation;
      ComPtr<abiC::IGlobalSystemMediaTransportControlsSessionMediaProperties> properties;
      if (SUCCEEDED(session->TryGetMediaPropertiesAsync(&propertiesOperation)) && waitFor(propertiesOperation.Get(), kOperationTimeoutMs, stopped))
        propertiesOperation->GetResults(&properties);
      if (properties)
      {
        HSTRING value{};
        if (SUCCEEDED(properties->get_Title(&value))) track.title = takeString(value);
        value = nullptr;
        if (SUCCEEDED(properties->get_Artist(&value))) track.artist = takeString(value);
        value = nullptr;
        if (SUCCEEDED(properties->get_AlbumTitle(&value))) track.album = takeString(value);
      }

      ComPtr<abiC::IGlobalSystemMediaTransportControlsSessionPlaybackInfo> playback;
      if (SUCCEEDED(session->GetPlaybackInfo(&playback)) && playback)
      {
        abiC::GlobalSystemMediaTransportControlsSessionPlaybackStatus status{};
        if (SUCCEEDED(playback->get_PlaybackStatus(&status))) track.playing = status == abiC::GlobalSystemMediaTransportControlsSessionPlaybackStatus_Playing;
        ComPtr<abiC::IGlobalSystemMediaTransportControlsSessionPlaybackControls> controls;
        if (SUCCEEDED(playback->get_Controls(&controls)) && controls)
        {
          boolean previous{}, play{}, pause{}, next{}, shuffle{}, repeat{};
          controls->get_IsPreviousEnabled(&previous);
          controls->get_IsPlayEnabled(&play);
          controls->get_IsPauseEnabled(&pause);
          controls->get_IsNextEnabled(&next);
          controls->get_IsShuffleEnabled(&shuffle);
          controls->get_IsRepeatEnabled(&repeat);
          track.canPrevious = previous != 0; track.canPlayPause = play != 0 || pause != 0; track.canNext = next != 0;
          track.canShuffle = shuffle != 0; track.canRepeat = repeat != 0;
        }
        ComPtr<abiF::IReference<bool>> shuffleActive;
        if (SUCCEEDED(playback->get_IsShuffleActive(&shuffleActive)) && shuffleActive)
        {
          boolean value{};
          if (SUCCEEDED(shuffleActive->get_Value(&value))) track.shuffleActive = value != 0;
        }
        ComPtr<abiF::IReference<abiM::MediaPlaybackAutoRepeatMode>> repeatMode;
        if (SUCCEEDED(playback->get_AutoRepeatMode(&repeatMode)) && repeatMode)
        {
          abiM::MediaPlaybackAutoRepeatMode value = abiM::MediaPlaybackAutoRepeatMode_None;
          if (SUCCEEDED(repeatMode->get_Value(&value)))
            track.repeatMode = value == abiM::MediaPlaybackAutoRepeatMode_Track ? 1 : value == abiM::MediaPlaybackAutoRepeatMode_List ? 2 : 0;
        }
      }

      ComPtr<abiC::IGlobalSystemMediaTransportControlsSessionTimelineProperties> timeline;
      if (SUCCEEDED(session->GetTimelineProperties(&timeline)) && timeline)
      {
        abiF::TimeSpan position{}, start{}, end{};
        timeline->get_Position(&position);
        timeline->get_StartTime(&start);
        timeline->get_EndTime(&end);
        track.position100ns = position.Duration;
        track.duration100ns = end.Duration - start.Duration;
      }

      // Re-read artwork when the track changes, and periodically afterwards
      // because some players publish the thumbnail after the metadata.
      const std::wstring key = track.sourceAppId + L"\n" + track.title + L"\n" + track.artist + L"\n" + track.album;
      const ULONGLONG now = GetTickCount64();
      if (properties && (key != artworkKey || artwork.empty() || now - artworkTick >= kArtworkRefreshMs))
      {
        std::vector<std::uint8_t> fresh;
        readArtwork(properties.Get(), fresh);
        if (!fresh.empty() || key != artworkKey) artwork = std::move(fresh);
        artworkKey = key;
        artworkTick = now;
      }
      track.artwork = artwork;
    }
    else
    {
      artworkKey.clear();
      artwork.clear();
    }
    formatTrackMetadata(track);
    track.revision = state.snapshot().revision + 1;
    state.update(std::move(track));
  }

  void execute(long commands)
  {
    auto session = currentSession();
    if (!session) return;
    ComPtr<abiC::IGlobalSystemMediaTransportControlsSessionPlaybackInfo> playback;
    ComPtr<abiC::IGlobalSystemMediaTransportControlsSessionPlaybackControls> controls;
    if (FAILED(session->GetPlaybackInfo(&playback)) || !playback || FAILED(playback->get_Controls(&controls)) || !controls) return;
    if (commands & CommandPrevious)
    {
      boolean enabled{};
      controls->get_IsPreviousEnabled(&enabled);
      ComPtr<abiF::IAsyncOperation<bool>> operation;
      if (enabled) runBoolOperation(operation, session->TrySkipPreviousAsync(&operation), stopped);
    }
    if (commands & CommandPlayPause)
    {
      abiC::GlobalSystemMediaTransportControlsSessionPlaybackStatus status{};
      playback->get_PlaybackStatus(&status);
      boolean canPlay{}, canPause{};
      controls->get_IsPlayEnabled(&canPlay);
      controls->get_IsPauseEnabled(&canPause);
      ComPtr<abiF::IAsyncOperation<bool>> operation;
      if (status == abiC::GlobalSystemMediaTransportControlsSessionPlaybackStatus_Playing) { if (canPause) runBoolOperation(operation, session->TryPauseAsync(&operation), stopped); }
      else if (canPlay) runBoolOperation(operation, session->TryPlayAsync(&operation), stopped);
    }
    if (commands & CommandNext)
    {
      boolean enabled{};
      controls->get_IsNextEnabled(&enabled);
      ComPtr<abiF::IAsyncOperation<bool>> operation;
      if (enabled) runBoolOperation(operation, session->TrySkipNextAsync(&operation), stopped);
    }
    if (commands & CommandShuffle)
    {
      boolean enabled{};
      controls->get_IsShuffleEnabled(&enabled);
      if (enabled)
      {
        boolean active{};
        ComPtr<abiF::IReference<bool>> value;
        if (SUCCEEDED(playback->get_IsShuffleActive(&value)) && value) value->get_Value(&active);
        ComPtr<abiF::IAsyncOperation<bool>> operation;
        runBoolOperation(operation, session->TryChangeShuffleActiveAsync(active ? 0 : 1, &operation), stopped);
      }
    }
    if (commands & CommandRepeat)
    {
      boolean enabled{};
      controls->get_IsRepeatEnabled(&enabled);
      if (enabled)
      {
        abiM::MediaPlaybackAutoRepeatMode mode = abiM::MediaPlaybackAutoRepeatMode_None;
        ComPtr<abiF::IReference<abiM::MediaPlaybackAutoRepeatMode>> value;
        if (SUCCEEDED(playback->get_AutoRepeatMode(&value)) && value) value->get_Value(&mode);
        const auto next = mode == abiM::MediaPlaybackAutoRepeatMode_None ? abiM::MediaPlaybackAutoRepeatMode_List
                        : mode == abiM::MediaPlaybackAutoRepeatMode_List ? abiM::MediaPlaybackAutoRepeatMode_Track
                        : abiM::MediaPlaybackAutoRepeatMode_None;
        ComPtr<abiF::IAsyncOperation<bool>> operation;
        runBoolOperation(operation, session->TryChangeAutoRepeatModeAsync(next, &operation), stopped);
      }
    }
  }
};
}

MediaSessionManager::~MediaSessionManager() { stop(); }

void MediaSessionManager::start()
{
  if (thread_) return;
  InterlockedExchange(&stopped_, 0);
  wake_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  thread_ = CreateThread(nullptr, 0, &MediaSessionManager::threadEntry, this, 0, nullptr);
}

void MediaSessionManager::queue(long command)
{
  InterlockedOr(&commands_, command);
  if (wake_) SetEvent(static_cast<HANDLE>(wake_));
}

void MediaSessionManager::poll() { queue(CommandRefresh); }
void MediaSessionManager::previous() { queue(CommandPrevious); }
void MediaSessionManager::togglePlayPause() { queue(CommandPlayPause); }
void MediaSessionManager::next() { queue(CommandNext); }
void MediaSessionManager::toggleShuffle() { queue(CommandShuffle); }
void MediaSessionManager::cycleRepeat() { queue(CommandRepeat); }

void MediaSessionManager::stop() noexcept
{
  InterlockedExchange(&stopped_, 1);
  if (wake_) SetEvent(static_cast<HANDLE>(wake_));
  if (thread_)
  {
    WaitForSingleObject(static_cast<HANDLE>(thread_), 10000);
    CloseHandle(static_cast<HANDLE>(thread_));
    thread_ = nullptr;
  }
  if (wake_) { CloseHandle(static_cast<HANDLE>(wake_)); wake_ = nullptr; }
}

unsigned long __stdcall MediaSessionManager::threadEntry(void* context)
{
  static_cast<MediaSessionManager*>(context)->run();
  return 0;
}

void MediaSessionManager::run()
{
  const HRESULT initialized = RoInitialize(RO_INIT_MULTITHREADED);
  {
    Worker worker{state_, &stopped_, {}, {}, {}, 0};
    while (!stopped_)
    {
      if (worker.manager || worker.connect()) break;
      WaitForSingleObject(static_cast<HANDLE>(wake_), 2000);
    }
    while (!stopped_)
    {
      const long commands = InterlockedExchange(&commands_, 0);
      if (commands & ~CommandRefresh) worker.execute(commands);
      if (stopped_) break;
      worker.refresh();
      WaitForSingleObject(static_cast<HANDLE>(wake_), kPollIntervalMs);
    }
  }
  if (SUCCEEDED(initialized)) RoUninitialize();
}
}
#endif
