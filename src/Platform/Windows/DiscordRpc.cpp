#include "DiscordRpc.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <windows.h>

namespace mo::win {
namespace {
std::string utf8(const std::wstring& value)
{
  if(value.empty()) return {};
  const int length=WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
  std::string result(static_cast<std::size_t>(std::max(length,0)),0);
  if(length)WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),result.data(),length,nullptr,nullptr);
  return result;
}

std::string escaped(const std::wstring& value)
{
  std::string result;
  for(unsigned char c:utf8(value))
  {
    switch(c){case '"':result+="\\\"";break;case '\\':result+="\\\\";break;case '\b':result+="\\b";break;case '\f':result+="\\f";break;case '\n':result+="\\n";break;case '\r':result+="\\r";break;case '\t':result+="\\t";break;default:if(c>=0x20)result.push_back(static_cast<char>(c));break;}
  }
  return result;
}

std::int64_t unixNow()
{
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
}

DiscordRpc::~DiscordRpc(){disconnect();}

void DiscordRpc::disconnect()
{
  HANDLE pipe=static_cast<HANDLE>(pipe_);
  if(pipe)CloseHandle(pipe);
  pipe_=nullptr;connectedApplicationId_.clear();lastSignature_.clear();lastRevision_=~std::uint64_t{};lastSentTick_=0;
}

bool DiscordRpc::sendFrame(std::uint32_t opcode,const std::string& json)
{
  HANDLE pipe=static_cast<HANDLE>(pipe_);if(!pipe)return false;
  struct Header{std::uint32_t opcode;std::uint32_t length;}header{opcode,static_cast<std::uint32_t>(json.size())};
  DWORD written{};
  if(!WriteFile(pipe,&header,sizeof(header),&written,nullptr)||written!=sizeof(header)||(!json.empty()&&(!WriteFile(pipe,json.data(),static_cast<DWORD>(json.size()),&written,nullptr)||written!=json.size()))){disconnect();return false;}
  return true;
}

bool DiscordRpc::connect(const std::wstring& applicationId)
{
  disconnect();
  for(int index=0;index<10;++index)
  {
    const std::wstring path=L"\\\\.\\pipe\\discord-ipc-"+std::to_wstring(index);
    HANDLE pipe=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);
    if(pipe==INVALID_HANDLE_VALUE)continue;
    pipe_=pipe;connectedApplicationId_=applicationId;
    const std::string handshake="{\"v\":1,\"client_id\":\""+escaped(applicationId)+"\"}";
    if(sendFrame(0,handshake))return true;
  }
  return false;
}

void DiscordRpc::drainResponses()
{
  HANDLE pipe=static_cast<HANDLE>(pipe_);if(!pipe)return;
  DWORD available{};std::array<char,4096> buffer{};
  while(PeekNamedPipe(pipe,nullptr,0,nullptr,&available,nullptr)&&available){DWORD read{};if(!ReadFile(pipe,buffer.data(),static_cast<DWORD>(std::min<std::size_t>(buffer.size(),available)),&read,nullptr)||!read){disconnect();return;}}
}

void DiscordRpc::update(const TrackInfo& track,const DiscordRpcSettings& settings)
{
  if(!settings.enabled||settings.applicationId.empty()){disconnect();return;}
  const auto nowTick=GetTickCount64();
  if(!pipe_||connectedApplicationId_!=settings.applicationId)
  {
    if(nowTick-lastAttemptTick_<5000)return;
    lastAttemptTick_=nowTick;if(!connect(settings.applicationId))return;
  }
  drainResponses();if(!pipe_)return;
  const std::wstring signature=track.title+L"\n"+track.artist+L"\n"+track.album+L"\n"+(track.playing?L"1":L"0")+L"\n"+std::to_wstring(track.duration100ns)+L"\n"+settings.defaultIconKey+L"\n"+settings.artworkKey;
  if(lastRevision_==track.revision||(signature==lastSignature_&&nowTick-lastSentTick_<15000))return;
  lastRevision_=track.revision;
  lastSignature_=signature;lastSentTick_=nowTick;

  const DWORD processId=GetCurrentProcessId();
  const std::wstring details=track.title.empty()?L"Nothing playing":track.title;
  const std::wstring state=track.title.empty()?L"Waiting for music":(track.artist.empty()?L"Unknown artist":track.artist);
  const std::wstring image=track.title.empty()||settings.artworkKey.empty()?settings.defaultIconKey:settings.artworkKey;
  std::string activity="{\"details\":\""+escaped(details)+"\",\"state\":\""+escaped(state)+"\",\"assets\":{\"large_image\":\""+escaped(image)+"\",\"large_text\":\""+escaped(track.album.empty()?details:track.album)+"\"}";
  if(track.playing&&track.duration100ns>0)
  {
    const auto now=unixNow();const auto elapsed=std::max<std::int64_t>(0,track.position100ns/10000000);const auto duration=std::max<std::int64_t>(1,track.duration100ns/10000000);
    activity+=",\"timestamps\":{\"start\":"+std::to_string(now-elapsed)+",\"end\":"+std::to_string(now-elapsed+duration)+"}";
  }
  activity+=",\"buttons\":[{\"label\":\"Trigon.Systems\",\"url\":\"https://trigon.systems\"},{\"label\":\"GitHub Repo\",\"url\":\"https://github.com/NihonFemStyle/Music-Framework\"}],\"instance\":false}";
  const std::string payload="{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":"+std::to_string(processId)+",\"activity\":"+activity+"},\"nonce\":\""+std::to_string(++nonce_)+"\"}";
  sendFrame(1,payload);
}

}
