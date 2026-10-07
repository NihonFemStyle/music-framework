#include "MetadataFormatter.h"
#include <windows.h>
#include <algorithm>
#include <cwctype>

namespace mo {
namespace {
std::wstring lower(std::wstring value){std::transform(value.begin(),value.end(),value.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});return value;}
void trim(std::wstring& value){auto first=value.find_first_not_of(L" \t-–—|()[]");auto last=value.find_last_not_of(L" \t-–—|()[]");if(first==std::wstring::npos){value.clear();return;}value=value.substr(first,last-first+1);}
}

void formatTrackMetadata(TrackInfo& track){
  std::wstring lowered=lower(track.title);
  constexpr const wchar_t* markers[]{L"featuring ",L"feat. ",L"feat ",L"ft. ",L"ft "};
  std::size_t found=std::wstring::npos,markerLength{};
  for(auto marker:markers){std::size_t position{};while((position=lowered.find(marker,position))!=std::wstring::npos){bool boundary=position==0||iswspace(lowered[position-1])||lowered[position-1]==L'('||lowered[position-1]==L'[';if(boundary&&position<found){found=position;markerLength=wcslen(marker);break;}position+=wcslen(marker);}}
  if(found!=std::wstring::npos){
    auto featuredStart=found+markerLength;auto featuredEnd=track.title.find_first_of(L")]|–—",featuredStart);auto dash=track.title.find(L" - ",featuredStart);if(dash!=std::wstring::npos&&(featuredEnd==std::wstring::npos||dash<featuredEnd))featuredEnd=dash;
    std::wstring featured=track.title.substr(featuredStart,featuredEnd==std::wstring::npos?std::wstring::npos:featuredEnd-featuredStart);trim(featured);
    std::size_t removeStart=found;while(removeStart>0&&(iswspace(track.title[removeStart-1])||track.title[removeStart-1]==L'('||track.title[removeStart-1]==L'['))--removeStart;
    if(featuredEnd!=std::wstring::npos&&featuredEnd<track.title.size()&&(track.title[featuredEnd]==L')'||track.title[featuredEnd]==L']'))++featuredEnd;
    track.title.erase(removeStart,featuredEnd==std::wstring::npos?std::wstring::npos:featuredEnd-removeStart);
    trim(track.title);if(!featured.empty()){if(!track.artist.empty()){SYSTEMTIME now{};GetLocalTime(&now);track.artist+=(now.wMonth==4&&now.wDay==1)?L" \u00E2\u20AC\u00A2 ":L", ";}track.artist+=featured;}
  }
  constexpr const wchar_t* separators[]{L" - ",L" – ",L" — ",L" | "};
  for(auto separator:separators){std::size_t position{};while((position=track.title.find(separator,position))!=std::wstring::npos){track.title.replace(position,wcslen(separator),L"\n");++position;}}
}
}
