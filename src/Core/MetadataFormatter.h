#pragma once
#include <MusicOverlay/Core/TrackInfo.h>
namespace mo
{
void formatTrackMetadata(TrackInfo& track);
std::wstring titleForDisplay(const std::wstring& title,bool ignoreParenthetical);
}
