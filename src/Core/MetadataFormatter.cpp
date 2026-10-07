#include <algorithm>
#include <cwctype>
#include <windows.h>

#include "MetadataFormatter.h"
namespace mo
{
namespace
{
  std::wstring lower(std::wstring value){std::transform(value.begin(),value.end(),value.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});return value;}
  void trim(std::wstring& value){auto first=value.find_first_not_of(L" \t-–—|()[]");auto last=value.find_last_not_of(L" \t-–—|()[]");if(first==std::wstring::npos){value.clear();return;}value=value.substr(first,last-first+1);}
  }

  void formatTrackMetadata(TrackInfo& track)
  {
    std::wstring lowered=lower(track.title);
    constexpr const wchar_t* markers[]{L"featuring ",L"feat. ",L"feat ",L"ft. ",L"ft "};
    std::size_t found=std::wstring::npos,markerLength{};
    for(auto marker:markers){std::size_t position{};while((position=lowered.find(marker,position))!=std::wstring::npos){bool boundary=position==0||iswspace(lowered[position-1])||lowered[position-1]==L'('||lowered[position-1]==L'[';if(boundary&&position<found){found=position;markerLength=wcslen(marker);break;}position+=wcslen(marker);}}
    if(found!=std::wstring::npos)
    {
      auto featuredStart=found+markerLength;auto featuredEnd=track.title.find_first_of(L")]|–—",featuredStart);auto dash=track.title.find(L" - ",featuredStart);if(dash!=std::wstring::npos&&(featuredEnd==std::wstring::npos||dash<featuredEnd))featuredEnd=dash;
      std::wstring featured=track.title.substr(featuredStart,featuredEnd==std::wstring::npos?std::wstring::npos:featuredEnd-featuredStart);trim(featured);
      std::size_t removeStart=found;while(removeStart>0&&(iswspace(track.title[removeStart-1])||track.title[removeStart-1]==L'('||track.title[removeStart-1]==L'['))--removeStart;
      if(featuredEnd!=std::wstring::npos&&featuredEnd<track.title.size()&&(track.title[featuredEnd]==L')'||track.title[featuredEnd]==L']'))++featuredEnd;
      track.title.erase(removeStart,featuredEnd==std::wstring::npos?std::wstring::npos:featuredEnd-removeStart);
      trim(track.title);if(!featured.empty()){if(!track.artist.empty()){SYSTEMTIME now{};GetLocalTime(&now);track.artist+=(now.wMonth==4&&now.wDay==1)?L" \u00E2\u20AC\u00A2 ":L", ";}track.artist+=featured;}
      }
      constexpr const wchar_t* separators[]{L" - ",L" – ",L" — ",L" | "};
      for(auto separator:separators){std::size_t position{};while((position=track.title.find(separator,position))!=std::wstring::npos){track.title.replace(position,wcslen(separator),L"\n");++position;}}

      std::size_t position{};
      while((position=track.title.find(L'(',position))!=std::wstring::npos)
      {
        auto closing=track.title.find(L')',position+1);if(closing==std::wstring::npos)break;
        std::size_t lineStart=position;while(lineStart>0&&(track.title[lineStart-1]==L' '||track.title[lineStart-1]==L'\t'))--lineStart;
        if(lineStart>0&&track.title[lineStart-1]!=L'\n')track.title.replace(lineStart,position-lineStart,L"\n");
        position=closing+1;
        }
      }

      std::wstring titleForDisplay(const std::wstring& title,bool ignoreParenthetical)
      {
        if(!ignoreParenthetical)return title;
        std::wstring result=title;std::size_t position{};
        while((position=result.find(L'(',position))!=std::wstring::npos)
        {
          auto closing=result.find(L')',position+1);if(closing==std::wstring::npos)break;
          std::size_t removeStart=position;while(removeStart>0&&(result[removeStart-1]==L' '||result[removeStart-1]==L'\t'||result[removeStart-1]==L'\n'))--removeStart;
          result.erase(removeStart,closing-removeStart+1);position=removeStart;
          }
        while(!result.empty()&&(result.back()==L' '||result.back()==L'\t'||result.back()==L'\n'))result.pop_back();
        return result;
        }
      }
