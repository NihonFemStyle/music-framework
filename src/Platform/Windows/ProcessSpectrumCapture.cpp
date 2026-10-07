#include "ProcessSpectrumCapture.h"

#include <windows.h>

#include <algorithm>
#include <appmodel.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <audiopolicy.h>
#include <cmath>
#include <cwctype>
#include <endpointvolume.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <vector>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
namespace mo::win
{
namespace
{
  std::wstring lower(std::wstring value){std::transform(value.begin(),value.end(),value.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});return value;}
  struct WindowSearch{std::wstring target;DWORD pid{};};
  BOOL CALLBACK findWindow(HWND hwnd,LPARAM value){auto& search=*reinterpret_cast<WindowSearch*>(value);DWORD pid{};GetWindowThreadProcessId(hwnd,&pid);if(!pid)return TRUE;HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return TRUE;wchar_t id[512]{};UINT length=_countof(id);bool match=GetApplicationUserModelId(process,&length,id)==ERROR_SUCCESS&&lower(id)==search.target;if(!match){wchar_t path[MAX_PATH]{};DWORD pathLength=_countof(path);if(QueryFullProcessImageNameW(process,0,path,&pathLength)){std::wstring name=lower(path);auto slash=name.find_last_of(L"\\/");if(slash!=std::wstring::npos)name.erase(0,slash+1);match=search.target.find(name)!=std::wstring::npos||name.find(search.target)!=std::wstring::npos;}}CloseHandle(process);if(match){search.pid=pid;return FALSE;}return TRUE;}
  bool processMatches(DWORD pid,const std::wstring& target){HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return false;wchar_t id[512]{};UINT length=_countof(id);bool match=GetApplicationUserModelId(process,&length,id)==ERROR_SUCCESS&&lower(id)==target;if(!match){wchar_t path[1024]{};DWORD pathLength=_countof(path);if(QueryFullProcessImageNameW(process,0,path,&pathLength)){std::wstring name=lower(path);auto slash=name.find_last_of(L"\\/");if(slash!=std::wstring::npos)name.erase(0,slash+1);match=target.find(name)!=std::wstring::npos||name.find(target)!=std::wstring::npos;}}CloseHandle(process);return match;}
  DWORD resolveAudioSession(const std::wstring& target){ComPtr<IMMDeviceEnumerator> devices;if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&devices))))return 0;ComPtr<IMMDevice> endpoint;if(FAILED(devices->GetDefaultAudioEndpoint(eRender,eMultimedia,&endpoint)))return 0;ComPtr<IAudioSessionManager2> manager;if(FAILED(endpoint->Activate(__uuidof(IAudioSessionManager2),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(manager.ReleaseAndGetAddressOf()))))return 0;ComPtr<IAudioSessionEnumerator> sessions;if(FAILED(manager->GetSessionEnumerator(&sessions)))return 0;int count{};sessions->GetCount(&count);DWORD bestPid{};float bestScore=-1.f;for(int index=0;index<count;++index){ComPtr<IAudioSessionControl> control;if(FAILED(sessions->GetSession(index,&control)))continue;ComPtr<IAudioSessionControl2> control2;if(FAILED(control.As(&control2)))continue;DWORD pid{};if(FAILED(control2->GetProcessId(&pid))||!pid||!processMatches(pid,target))continue;AudioSessionState state=AudioSessionStateInactive;control->GetState(&state);float peak{};ComPtr<IAudioMeterInformation> meter;if(SUCCEEDED(control.As(&meter)))meter->GetPeakValue(&peak);float score=peak+(state==AudioSessionStateActive?2.f:(state==AudioSessionStateInactive?1.f:0.f));if(score>bestScore){bestScore=score;bestPid=pid;}}return bestPid;}
  DWORD resolveProcess(const std::wstring& appId){auto target=lower(appId);if(DWORD pid=resolveAudioSession(target))return pid;WindowSearch search{target,0};EnumWindows(findWindow,reinterpret_cast<LPARAM>(&search));return search.pid;}

  class ActivationHandler final:public IActivateAudioInterfaceCompletionHandler,public IAgileObject
  {
    public:
    ActivationHandler():event_(CreateEventW(nullptr,TRUE,FALSE,nullptr)){}
    ~ActivationHandler(){if(event_)CloseHandle(event_);}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** object) override{if(!object)return E_POINTER;*object=nullptr;if(iid==__uuidof(IUnknown)||iid==__uuidof(IActivateAudioInterfaceCompletionHandler))*object=static_cast<IActivateAudioInterfaceCompletionHandler*>(this);else if(iid==__uuidof(IAgileObject))*object=static_cast<IAgileObject*>(this);else return E_NOINTERFACE;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs_);}
    ULONG STDMETHODCALLTYPE Release() override{ULONG value=InterlockedDecrement(&refs_);if(!value)delete this;return value;}
    HRESULT STDMETHODCALLTYPE ActivateCompleted(IActivateAudioInterfaceAsyncOperation* operation) override{ComPtr<IUnknown> object;operation->GetActivateResult(&result_,&object);if(SUCCEEDED(result_))object.As(&client_);SetEvent(event_);return S_OK;}
    HANDLE event()const{return event_;}HRESULT result()const{return result_;}IAudioClient* client()const{return client_.Get();}
    private:LONG refs_{1};HANDLE event_{};HRESULT result_{E_PENDING};ComPtr<IAudioClient> client_;
    };
    }

    bool ProcessSpectrumCapture::startForApp(const std::wstring& appId)
    {
      if(appId.empty()){stop();return false;}DWORD pid=resolveProcess(appId);if(!pid)return false;return startForProcess(appId,pid);
      }

      bool ProcessSpectrumCapture::ensureForApp(const std::wstring& appId)
      {
        if(appId.empty()){if(!appId_.empty())stop();return false;}
        DWORD pid=resolveProcess(appId);if(!pid)return false;
        if(appId==appId_&&pid==processId_&&!captureFailed_.load())return true;
        return startForProcess(appId,pid);
        }

        bool ProcessSpectrumCapture::startForProcess(const std::wstring& appId,std::uint32_t pid)
        {
          stop();
          AUDIOCLIENT_ACTIVATION_PARAMS params{};params.ActivationType=AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;params.ProcessLoopbackParams.TargetProcessId=pid;params.ProcessLoopbackParams.ProcessLoopbackMode=PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
          PROPVARIANT activation{};activation.vt=VT_BLOB;activation.blob.cbSize=sizeof(params);activation.blob.pBlobData=reinterpret_cast<BYTE*>(&params);
          auto* handler=new ActivationHandler;ComPtr<IActivateAudioInterfaceAsyncOperation> operation;HRESULT hr=ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,__uuidof(IAudioClient),&activation,handler,&operation);if(FAILED(hr)){handler->Release();return false;}WaitForSingleObject(handler->event(),5000);if(FAILED(handler->result())||!handler->client()){handler->Release();return false;}
          ComPtr<IAudioClient> client=handler->client();handler->Release();
          WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;format.nSamplesPerSec=44100;format.wBitsPerSample=16;format.nBlockAlign=static_cast<WORD>(format.nChannels*format.wBitsPerSample/8);format.nAvgBytesPerSec=format.nSamplesPerSec*format.nBlockAlign;
          floatSamples_=false;sampleRate_=format.nSamplesPerSec;channels_=format.nChannels;
          REFERENCE_TIME duration=0;hr=client->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK|AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,duration,0,&format,nullptr);if(FAILED(hr))return false;
          ComPtr<IAudioCaptureClient> capture;if(FAILED(client->GetService(IID_PPV_ARGS(&capture)))||FAILED(client->Start()))return false;
          audioClient_=client.Detach();captureClient_=capture.Detach();sampleEvent_=nullptr;appId_=appId;processId_=pid;captureFailed_=false;stopping_=false;thread_=std::thread([this]{captureLoop();});return true;
          }

          void ProcessSpectrumCapture::stop(){stopping_=true;if(sampleEvent_)SetEvent(static_cast<HANDLE>(sampleEvent_));if(thread_.joinable())thread_.join();if(audioClient_)static_cast<IAudioClient*>(audioClient_)->Stop();if(captureClient_){static_cast<IAudioCaptureClient*>(captureClient_)->Release();captureClient_=nullptr;}if(audioClient_){static_cast<IAudioClient*>(audioClient_)->Release();audioClient_=nullptr;}if(sampleEvent_){CloseHandle(static_cast<HANDLE>(sampleEvent_));sampleEvent_=nullptr;}appId_.clear();processId_=0;captureFailed_=false;for(auto& band:bands_)band=0.f;}

          void ProcessSpectrumCapture::captureLoop()
          {
            CoInitializeEx(nullptr,COINIT_MULTITHREADED);auto* capture=static_cast<IAudioCaptureClient*>(captureClient_);std::vector<float> samples;samples.reserve(2048);
            while(!stopping_){Sleep(10);UINT32 packets{};HRESULT packetResult=capture->GetNextPacketSize(&packets);if(FAILED(packetResult)){captureFailed_=true;break;}while(packets){BYTE* data{};UINT32 frames{};DWORD flags{};if(FAILED(capture->GetBuffer(&data,&frames,&flags,nullptr,nullptr))){captureFailed_=true;break;}if(!(flags&AUDCLNT_BUFFERFLAGS_SILENT)){for(UINT32 frame=0;frame<frames;++frame){float mono=0.f;for(UINT16 channel=0;channel<channels_;++channel)mono+=floatSamples_?reinterpret_cast<float*>(data)[frame*channels_+channel]:reinterpret_cast<short*>(data)[frame*channels_+channel]/32768.f;samples.push_back(mono/std::max<UINT16>(1,channels_));}}capture->ReleaseBuffer(frames);packetResult=capture->GetNextPacketSize(&packets);if(FAILED(packetResult)){captureFailed_=true;break;}}if(captureFailed_)break;
              if(samples.size()<512)continue;if(samples.size()>2048)samples.erase(samples.begin(),samples.end()-2048);std::size_t count=std::min<std::size_t>(1024,samples.size()),offset=samples.size()-count;
              std::array<float,BandCount> levels{};float peak=.0005f;
              for(std::size_t band=0;band<BandCount;++band){double frequency=45.0*std::pow(16000.0/45.0,double(band)/double(BandCount-1));double real=0,imag=0;for(std::size_t i=0;i<count;++i){double window=.5-.5*std::cos(6.283185307179586*double(i)/double(count-1));double angle=6.283185307179586*frequency*double(i)/double(sampleRate_);double value=samples[offset+i]*window;real+=value*std::cos(angle);imag-=value*std::sin(angle);}levels[band]=float(std::sqrt(real*real+imag*imag)/double(count));peak=std::max(peak,levels[band]);}
              const auto response=response_.load();const float attackOld=response==0?.05f:response==2?.58f:.2f;const float releaseOld=response==0?.45f:response==2?.91f:.76f;
              for(std::size_t band=0;band<BandCount;++band){float level=std::clamp(std::sqrt(levels[band]/peak),0.f,1.f);float old=bands_[band].load();bands_[band]=level>old?old*attackOld+level*(1.f-attackOld):old*releaseOld+level*(1.f-releaseOld);}
              }CoUninitialize();
              }

              std::array<float,ProcessSpectrumCapture::BandCount> ProcessSpectrumCapture::snapshot()const{std::array<float,BandCount> result{};for(std::size_t i=0;i<BandCount;++i)result[i]=bands_[i].load();return result;}
              }
